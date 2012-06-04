/**
 * AES CCM routines supporting the Power 7+ Nest Accelerators driver
 *
 * Copyright (C) 2012 International Business Machines Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Kent Yoder <yoder1@us.ibm.com>
 */

#include <crypto/internal/aead.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <crypto/scatterwalk.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <asm/vio.h>

#include "nx_csbcpb.h"
#include "nx.h"


static int ccm_aes_nx_set_key(struct crypto_aead *tfm,
			      const u8           *in_key,
			      unsigned int        key_len)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(&tfm->base);
	struct nx_csbcpb *csbcpb = nx_ctx->csbcpb;
	struct nx_csbcpb *csbcpb_aead = nx_ctx->csbcpb_aead;

	nx_ctx_init(nx_ctx, HCOP_FC_AES);

	switch (key_len) {
	case AES_KEYSIZE_128:
		NX_CPB_SET_KEY_SIZE(csbcpb, NX_KS_AES_128);
		NX_CPB_SET_KEY_SIZE(csbcpb_aead, NX_KS_AES_128);
		nx_ctx->ap = &nx_ctx->props[NX_PROPS_AES_128];
		break;
	default:
		return -EINVAL;
	}

	csbcpb->cpb.hdr.mode = NX_MODE_AES_CCM;
	memcpy(csbcpb->cpb.aes_ccm.key, in_key, key_len);

	csbcpb_aead->cpb.hdr.mode = NX_MODE_AES_CCA;
	memcpy(csbcpb_aead->cpb.aes_cca.key, in_key, key_len);

	return 0;

}

static int ccm4309_aes_nx_set_key(struct crypto_aead *tfm,
				  const u8           *in_key,
				  unsigned int        key_len)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(&tfm->base);

	if (key_len < 3)
		return -EINVAL;

	key_len -= 3;

	memcpy(nx_ctx->priv.ccm.nonce, in_key + key_len, 3);

	return ccm_aes_nx_set_key(tfm, in_key, key_len);
}

static int ccm_aes_nx_setauthsize(struct crypto_aead *tfm,
				  unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	crypto_aead_crt(tfm)->authsize = authsize;

	return 0;
}

static int ccm4309_aes_nx_setauthsize(struct crypto_aead *tfm,
				      unsigned int authsize)
{
	switch (authsize) {
	case 8:
	case 12:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	crypto_aead_crt(tfm)->authsize = authsize;

	return 0;
}

/* taken from crypto/ccm.c */
static int set_msg_len(u8 *block, unsigned int msglen, int csize)
{
	__be32 data;

	memset(block, 0, csize);
	block += csize;

	if (csize >= 4)
		csize = 4;
	else if (msglen > (unsigned int)(1 << (8 * csize)))
		return -EOVERFLOW;

	data = cpu_to_be32(msglen);
	memcpy(block - csize, (u8 *)&data + 4 - csize, csize);

	return 0;
}

/* taken from crypto/ccm.c */
static inline int crypto_ccm_check_iv(const u8 *iv)
{
	/* 2 <= L <= 8, so 1 <= L' <= 7. */
	if (1 > iv[0] || iv[0] > 7)
		return -EINVAL;

	return 0;
}

/* based on code from crypto/ccm.c */
static int generate_b0(u8 *iv, unsigned int assoclen, unsigned int authsize,
		       unsigned int cryptlen, u8 *b0)
{
	unsigned int l, lp, m = authsize;
	int rc;

	memcpy(b0, iv, 16);

	lp = b0[0];
	l = lp + 1;

	/* set m, bits 3-5 */
	*b0 |= (8 * ((m - 2) / 2));

	/* set adata, bit 6, if associated data is used */
	if (assoclen)
		*b0 |= 64;

	rc = set_msg_len(b0 + 16 - l, cryptlen, l);

	return rc;
}

static int generate_pat(u8                   *iv,
			struct aead_request  *req,
			struct nx_crypto_ctx *nx_ctx,
			unsigned int          authsize,
			unsigned int          nbytes,
			u8                   *out)
{
	struct nx_sg *nx_insg = nx_ctx->in_sg;
	struct nx_sg *nx_outsg = nx_ctx->out_sg;
	unsigned int iauth_len = 0;
	struct vio_pfo_op *op = NULL;
	u8 tmp[16], *b1 = NULL, *b0 = NULL, *result = NULL;
	int rc;

	/* zero the ctr value */
	memset(iv + 15 - iv[0], 0, iv[0] + 1);

	if (!req->assoclen) {
		b0 = nx_ctx->csbcpb->cpb.aes_ccm.in_pat_or_b0;
	} else if (req->assoclen <= 14) {
		/* if associated data is 14 bytes or less, we do 1 GCM
		 * operation on 2 AES blocks, B0 (stored in the csbcpb) and B1,
		 * which is fed in through the source buffers here */
		b0 = nx_ctx->csbcpb->cpb.aes_ccm.in_pat_or_b0;
		b1 = nx_ctx->priv.ccm.iauth_tag;
		iauth_len = req->assoclen;

		nx_insg = nx_build_sg_list(nx_insg, b1, 16, nx_ctx->ap->sglen);
		nx_outsg = nx_build_sg_list(nx_outsg, tmp, 16,
					    nx_ctx->ap->sglen);

		/* inlen should be negative, indicating to phyp that its a
		 * pointer to an sg list */
		nx_ctx->op.inlen = (nx_ctx->in_sg - nx_insg) *
					sizeof(struct nx_sg);
		nx_ctx->op.outlen = (nx_ctx->out_sg - nx_outsg) *
					sizeof(struct nx_sg);

		NX_CPB_FDM(nx_ctx->csbcpb) |= NX_FDM_ENDE_ENCRYPT;
		NX_CPB_FDM(nx_ctx->csbcpb) |= NX_FDM_INTERMEDIATE;

		op = &nx_ctx->op;
		result = nx_ctx->csbcpb->cpb.aes_ccm.out_pat_or_mac;
	} else if (req->assoclen <= 65280) {
		/* if associated data is less than (2^16 - 2^8), we construct
		 * B1 differently and feed in the associated data to a CCA
		 * operation */
		b0 = nx_ctx->csbcpb_aead->cpb.aes_cca.b0;
		b1 = nx_ctx->csbcpb_aead->cpb.aes_cca.b1;
		iauth_len = 14;

		/* remaining assoc data must have scatterlist built for it */
		nx_insg = nx_walk_and_build(nx_insg, nx_ctx->ap->sglen,
					    req->assoc, iauth_len,
					    req->assoclen - iauth_len);
		nx_ctx->op_aead.inlen = (nx_ctx->in_sg - nx_insg) *
						sizeof(struct nx_sg);

		op = &nx_ctx->op_aead;
		result = nx_ctx->csbcpb_aead->cpb.aes_cca.out_pat_or_b0;
	} else {
		/* if associated data is less than (2^32), we construct B1
		 * differently yet again and feed in the associated data to a
		 * CCA operation */
		pr_err("associated data len is %u bytes (returning -EINVAL)\n",
		       req->assoclen);
		rc = -EINVAL;
	}

	rc = generate_b0(iv, req->assoclen, authsize, nbytes, b0);
	if (rc)
		goto done;

	if (b1) {
		memset(b1, 0, 16);
		*(u16 *)b1 = (u16)req->assoclen;

		scatterwalk_map_and_copy(b1 + 2, req->assoc, 0,
					 iauth_len, SCATTERWALK_FROM_SG);

		rc = nx_hcall_sync(nx_ctx, op,
				   req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP);
		if (rc)
			goto done;

		atomic_inc(&(nx_ctx->stats->aes_ops));
		atomic64_add(req->assoclen, &(nx_ctx->stats->aes_bytes));

		memcpy(out, result, AES_BLOCK_SIZE);
	}
done:
	return rc;
}

static int ccm_nx_decrypt(struct aead_request   *req,
			  struct blkcipher_desc *desc)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(req->base.tfm);
	struct nx_csbcpb *csbcpb = nx_ctx->csbcpb;
	unsigned int nbytes = req->cryptlen;
	unsigned int authsize = crypto_aead_authsize(crypto_aead_reqtfm(req));
	struct nx_ccm_priv *priv = &nx_ctx->priv.ccm;
	int rc = -1;

	if (nbytes > nx_ctx->ap->databytelen)
		return -EINVAL;

	nbytes -= authsize;

	/* copy out the auth tag to compare with later */
	scatterwalk_map_and_copy(priv->oauth_tag,
				 req->src, nbytes, authsize,
				 SCATTERWALK_FROM_SG);

	rc = generate_pat(desc->info, req, nx_ctx, authsize, nbytes,
			  csbcpb->cpb.aes_ccm.in_pat_or_b0);
	if (rc)
		goto out;

	rc = nx_build_sg_lists(nx_ctx, desc, req->dst, req->src, nbytes,
			       csbcpb->cpb.aes_ccm.iv_or_ctr);
	if (rc)
		goto out;

	NX_CPB_FDM(nx_ctx->csbcpb) &= ~NX_FDM_ENDE_ENCRYPT;
	NX_CPB_FDM(nx_ctx->csbcpb) &= ~NX_FDM_INTERMEDIATE;

	rc = nx_hcall_sync(nx_ctx, &nx_ctx->op,
			   req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP);
	if (rc)
		goto out;

	atomic_inc(&(nx_ctx->stats->aes_ops));
	atomic64_add(csbcpb->csb.processed_byte_count,
		     &(nx_ctx->stats->aes_bytes));

	rc = memcmp(csbcpb->cpb.aes_ccm.out_pat_or_mac, priv->oauth_tag,
		    authsize) ? -EBADMSG : 0;
out:
	return rc;
}

static int ccm_nx_encrypt(struct aead_request   *req,
			  struct blkcipher_desc *desc)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(req->base.tfm);
	struct nx_csbcpb *csbcpb = nx_ctx->csbcpb;
	unsigned int nbytes = req->cryptlen;
	unsigned int authsize = crypto_aead_authsize(crypto_aead_reqtfm(req));
	int rc = -1;

	if (nbytes > nx_ctx->ap->databytelen)
		return -EINVAL;

	rc = generate_pat(desc->info, req, nx_ctx, authsize, nbytes,
			  csbcpb->cpb.aes_ccm.in_pat_or_b0);
	if (rc)
		goto out;

	rc = nx_build_sg_lists(nx_ctx, desc, req->dst, req->src, nbytes,
			       csbcpb->cpb.aes_ccm.iv_or_ctr);
	if (rc)
		goto out;

	NX_CPB_FDM(csbcpb) |= NX_FDM_ENDE_ENCRYPT;
	NX_CPB_FDM(csbcpb) &= ~NX_FDM_INTERMEDIATE;

	rc = nx_hcall_sync(nx_ctx, &nx_ctx->op,
			   req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP);
	if (rc)
		goto out;

	atomic_inc(&(nx_ctx->stats->aes_ops));
	atomic64_add(csbcpb->csb.processed_byte_count,
		     &(nx_ctx->stats->aes_bytes));

	/* copy out the auth tag */
	scatterwalk_map_and_copy(csbcpb->cpb.aes_ccm.out_pat_or_mac,
				 req->dst, nbytes, authsize,
				 SCATTERWALK_TO_SG);
out:
	return rc;
}

static int ccm4309_aes_nx_encrypt(struct aead_request *req)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(req->base.tfm);
	struct blkcipher_desc desc;
	u8 *iv = nx_ctx->priv.ccm.iv;

	iv[0] = 3;
	memcpy(iv + 1, nx_ctx->priv.ccm.nonce, 3);
	memcpy(iv + 4, req->iv, 8);

	desc.info = iv;
	desc.tfm = (struct crypto_blkcipher *)req->base.tfm;

	return ccm_nx_encrypt(req, &desc);
}

static int ccm_aes_nx_encrypt(struct aead_request *req)
{
	struct blkcipher_desc desc;
	int rc;

	desc.info = req->iv;
	desc.tfm = (struct crypto_blkcipher *)req->base.tfm;

	rc = crypto_ccm_check_iv(desc.info);
	if (rc)
		return rc;

	return ccm_nx_encrypt(req, &desc);
}

static int ccm4309_aes_nx_decrypt(struct aead_request *req)
{
	struct nx_crypto_ctx *nx_ctx = crypto_tfm_ctx(req->base.tfm);
	struct blkcipher_desc desc;
	u8 *iv = nx_ctx->priv.ccm.iv;

	iv[0] = 3;
	memcpy(iv + 1, nx_ctx->priv.ccm.nonce, 3);
	memcpy(iv + 4, req->iv, 8);

	desc.info = iv;
	desc.tfm = (struct crypto_blkcipher *)req->base.tfm;

	return ccm_nx_decrypt(req, &desc);
}

static int ccm_aes_nx_decrypt(struct aead_request *req)
{
	struct blkcipher_desc desc;
	int rc;

	desc.info = req->iv;
	desc.tfm = (struct crypto_blkcipher *)req->base.tfm;

	rc = crypto_ccm_check_iv(desc.info);
	if (rc)
		return rc;

	return ccm_nx_decrypt(req, &desc);
}

/* tell the block cipher walk routines that this is a stream cipher by
 * setting cra_blocksize to 1. Even using blkcipher_walk_virt_block
 * during encrypt/decrypt doesn't solve this problem, because it calls
 * blkcipher_walk_done under the covers, which doesn't use walk->blocksize,
 * but instead uses this tfm->blocksize. */
struct crypto_alg nx_ccm_aes_alg = {
	.cra_name        = "ccm(aes)",
	.cra_driver_name = "ccm-aes-nx",
	.cra_priority    = 300,
	.cra_flags       = CRYPTO_ALG_TYPE_AEAD |
			   CRYPTO_ALG_NEED_FALLBACK,
	.cra_blocksize   = 1,
	.cra_ctxsize     = sizeof(struct nx_crypto_ctx),
	.cra_type        = &crypto_aead_type,
	.cra_module      = THIS_MODULE,
	.cra_list        = LIST_HEAD_INIT(nx_ccm_aes_alg.cra_list),
	.cra_init        = nx_crypto_ctx_aes_ccm_init,
	.cra_exit        = nx_crypto_ctx_exit,
	.cra_aead = {
		.ivsize      = AES_BLOCK_SIZE,
		.maxauthsize = AES_BLOCK_SIZE,
		.setkey      = ccm_aes_nx_set_key,
		.setauthsize = ccm_aes_nx_setauthsize,
		.encrypt     = ccm_aes_nx_encrypt,
		.decrypt     = ccm_aes_nx_decrypt,
	}
};

struct crypto_alg nx_ccm4309_aes_alg = {
	.cra_name        = "rfc4309(ccm(aes))",
	.cra_driver_name = "rfc4309-ccm-aes-nx",
	.cra_priority    = 300,
	.cra_flags       = CRYPTO_ALG_TYPE_AEAD |
			   CRYPTO_ALG_NEED_FALLBACK,
	.cra_blocksize   = 1,
	.cra_ctxsize     = sizeof(struct nx_crypto_ctx),
	.cra_type        = &crypto_nivaead_type,
	.cra_module      = THIS_MODULE,
	.cra_list        = LIST_HEAD_INIT(nx_ccm4309_aes_alg.cra_list),
	.cra_init        = nx_crypto_ctx_aes_ccm_init,
	.cra_exit        = nx_crypto_ctx_exit,
	.cra_aead = {
		.ivsize      = 8,
		.maxauthsize = AES_BLOCK_SIZE,
		.setkey      = ccm4309_aes_nx_set_key,
		.setauthsize = ccm4309_aes_nx_setauthsize,
		.encrypt     = ccm4309_aes_nx_encrypt,
		.decrypt     = ccm4309_aes_nx_decrypt,
		.geniv       = "seqiv",
	}
};
