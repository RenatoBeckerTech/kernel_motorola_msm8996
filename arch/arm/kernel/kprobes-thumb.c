/*
 * arch/arm/kernel/kprobes-thumb.c
 *
 * Copyright (C) 2011 Jon Medhurst <tixy@yxit.co.uk>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>

#include "kprobes.h"


/*
 * True if current instruction is in an IT block.
 */
#define in_it_block(cpsr)	((cpsr & 0x06000c00) != 0x00000000)

/*
 * Return the condition code to check for the currently executing instruction.
 * This is in ITSTATE<7:4> which is in CPSR<15:12> but is only valid if
 * in_it_block returns true.
 */
#define current_cond(cpsr)	((cpsr >> 12) & 0xf)

/*
 * Return the PC value for a probe in thumb code.
 * This is the address of the probed instruction plus 4.
 * We subtract one because the address will have bit zero set to indicate
 * a pointer to thumb code.
 */
static inline unsigned long __kprobes thumb_probe_pc(struct kprobe *p)
{
	return (unsigned long)p->addr - 1 + 4;
}

static void __kprobes
t32_simulate_table_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	unsigned long rnv = (rn == 15) ? pc : regs->uregs[rn];
	unsigned long rmv = regs->uregs[rm];
	unsigned int halfwords;

	if (insn & 0x10) /* TBH */
		halfwords = ((u16 *)rnv)[rmv];
	else /* TBB */
		halfwords = ((u8 *)rnv)[rmv];

	regs->ARM_pc = pc + 2 * halfwords;
}

static void __kprobes
t32_simulate_mrs(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	unsigned long mask = 0xf8ff03df; /* Mask out execution state */
	regs->uregs[rd] = regs->ARM_cpsr & mask;
}

static void __kprobes
t32_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);

	long offset = insn & 0x7ff;		/* imm11 */
	offset += (insn & 0x003f0000) >> 5;	/* imm6 */
	offset += (insn & 0x00002000) << 4;	/* J1 */
	offset += (insn & 0x00000800) << 7;	/* J2 */
	offset -= (insn & 0x04000000) >> 7;	/* Apply sign bit */

	regs->ARM_pc = pc + (offset * 2);
}

static enum kprobe_insn __kprobes
t32_decode_cond_branch(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	int cc = (insn >> 22) & 0xf;
	asi->insn_check_cc = kprobe_condition_checks[cc];
	asi->insn_handler = t32_simulate_cond_branch;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t32_simulate_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);

	long offset = insn & 0x7ff;		/* imm11 */
	offset += (insn & 0x03ff0000) >> 5;	/* imm10 */
	offset += (insn & 0x00002000) << 9;	/* J1 */
	offset += (insn & 0x00000800) << 10;	/* J2 */
	if (insn & 0x04000000)
		offset -= 0x00800000; /* Apply sign bit */
	else
		offset ^= 0x00600000; /* Invert J1 and J2 */

	if (insn & (1 << 14)) {
		/* BL or BLX */
		regs->ARM_lr = (unsigned long)p->addr + 4;
		if (!(insn & (1 << 12))) {
			/* BLX so switch to ARM mode */
			regs->ARM_cpsr &= ~PSR_T_BIT;
			pc &= ~3;
		}
	}

	regs->ARM_pc = pc + (offset * 2);
}

static void __kprobes
t32_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long addr = thumb_probe_pc(p) & ~3;
	int rt = (insn >> 12) & 0xf;
	unsigned long rtv;

	long offset = insn & 0xfff;
	if (insn & 0x00800000)
		addr += offset;
	else
		addr -= offset;

	if (insn & 0x00400000) {
		/* LDR */
		rtv = *(unsigned long *)addr;
		if (rt == 15) {
			bx_write_pc(rtv, regs);
			return;
		}
	} else if (insn & 0x00200000) {
		/* LDRH */
		if (insn & 0x01000000)
			rtv = *(s16 *)addr;
		else
			rtv = *(u16 *)addr;
	} else {
		/* LDRB */
		if (insn & 0x01000000)
			rtv = *(s8 *)addr;
		else
			rtv = *(u8 *)addr;
	}

	regs->uregs[rt] = rtv;
}

static enum kprobe_insn __kprobes
t32_decode_ldmstm(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	enum kprobe_insn ret = kprobe_decode_ldmstm(insn, asi);

	/* Fixup modified instruction to have halfwords in correct order...*/
	insn = asi->insn[0];
	((u16 *)asi->insn)[0] = insn >> 16;
	((u16 *)asi->insn)[1] = insn & 0xffff;

	return ret;
}

static void __kprobes
t32_emulate_ldrdstrd(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p) & ~3;
	int rt1 = (insn >> 12) & 0xf;
	int rt2 = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;

	register unsigned long rt1v asm("r0") = regs->uregs[rt1];
	register unsigned long rt2v asm("r1") = regs->uregs[rt2];
	register unsigned long rnv asm("r2") = (rn == 15) ? pc
							  : regs->uregs[rn];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rt1v), "=r" (rt2v), "=r" (rnv)
		: "0" (rt1v), "1" (rt2v), "2" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	if (rn != 15)
		regs->uregs[rn] = rnv; /* Writeback base register */
	regs->uregs[rt1] = rt1v;
	regs->uregs[rt2] = rt2v;
}

static void __kprobes
t32_emulate_ldrstr(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rt = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	register unsigned long rtv asm("r0") = regs->uregs[rt];
	register unsigned long rnv asm("r2") = regs->uregs[rn];
	register unsigned long rmv asm("r3") = regs->uregs[rm];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rtv), "=r" (rnv)
		: "0" (rtv), "1" (rnv), "r" (rmv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rn] = rnv; /* Writeback base register */
	if (rt == 15) /* Can't be true for a STR as they aren't allowed */
		bx_write_pc(rtv, regs);
	else
		regs->uregs[rt] = rtv;
}

static void __kprobes
t32_emulate_rd8rn16rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = regs->uregs[rn];
	register unsigned long rmv asm("r3") = regs->uregs[rm];
	unsigned long cpsr = regs->ARM_cpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"blx    %[fn]			\n\t"
		"mrs	%[cpsr], cpsr		\n\t"
		: "=r" (rdv), [cpsr] "=r" (cpsr)
		: "0" (rdv), "r" (rnv), "r" (rmv),
		  "1" (cpsr), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
	regs->ARM_cpsr = (regs->ARM_cpsr & ~APSR_MASK) | (cpsr & APSR_MASK);
}

static void __kprobes
t32_emulate_rd8pc16_noflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rd = (insn >> 8) & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = pc & ~3;

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rdv)
		: "0" (rdv), "r" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
}

static void __kprobes
t32_emulate_rd8rn16_noflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = regs->uregs[rn];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rdv)
		: "0" (rdv), "r" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
}

static const union decode_item t32_table_1110_100x_x0xx[] = {
	/* Load/store multiple instructions */

	/* Rn is PC		1110 100x x0xx 1111 xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfe4f0000, 0xe80f0000),

	/* SRS			1110 1000 00x0 xxxx xxxx xxxx xxxx xxxx */
	/* RFE			1110 1000 00x1 xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffc00000, 0xe8000000),
	/* SRS			1110 1001 10x0 xxxx xxxx xxxx xxxx xxxx */
	/* RFE			1110 1001 10x1 xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffc00000, 0xe9800000),

	/* STM Rn, {...pc}	1110 100x x0x0 xxxx 1xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfe508000, 0xe8008000),
	/* LDM Rn, {...lr,pc}	1110 100x x0x1 xxxx 11xx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfe50c000, 0xe810c000),
	/* LDM/STM Rn, {...sp}	1110 100x x0xx xxxx xx1x xxxx xxxx xxxx */
	DECODE_REJECT	(0xfe402000, 0xe8002000),

	/* STMIA		1110 1000 10x0 xxxx xxxx xxxx xxxx xxxx */
	/* LDMIA		1110 1000 10x1 xxxx xxxx xxxx xxxx xxxx */
	/* STMDB		1110 1001 00x0 xxxx xxxx xxxx xxxx xxxx */
	/* LDMDB		1110 1001 00x1 xxxx xxxx xxxx xxxx xxxx */
	DECODE_CUSTOM	(0xfe400000, 0xe8000000, t32_decode_ldmstm),

	DECODE_END
};

static const union decode_item t32_table_1110_100x_x1xx[] = {
	/* Load/store dual, load/store exclusive, table branch */

	/* STRD (immediate)	1110 1000 x110 xxxx xxxx xxxx xxxx xxxx */
	/* LDRD (immediate)	1110 1000 x111 xxxx xxxx xxxx xxxx xxxx */
	DECODE_OR	(0xff600000, 0xe8600000),
	/* STRD (immediate)	1110 1001 x1x0 xxxx xxxx xxxx xxxx xxxx */
	/* LDRD (immediate)	1110 1001 x1x1 xxxx xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xff400000, 0xe9400000, t32_emulate_ldrdstrd,
						 REGS(NOPCWB, NOSPPC, NOSPPC, 0, 0)),

	/* TBB			1110 1000 1101 xxxx xxxx xxxx 0000 xxxx */
	/* TBH			1110 1000 1101 xxxx xxxx xxxx 0001 xxxx */
	DECODE_SIMULATEX(0xfff000e0, 0xe8d00000, t32_simulate_table_branch,
						 REGS(NOSP, 0, 0, 0, NOSPPC)),

	/* STREX		1110 1000 0100 xxxx xxxx xxxx xxxx xxxx */
	/* LDREX		1110 1000 0101 xxxx xxxx xxxx xxxx xxxx */
	/* STREXB		1110 1000 1100 xxxx xxxx xxxx 0100 xxxx */
	/* STREXH		1110 1000 1100 xxxx xxxx xxxx 0101 xxxx */
	/* STREXD		1110 1000 1100 xxxx xxxx xxxx 0111 xxxx */
	/* LDREXB		1110 1000 1101 xxxx xxxx xxxx 0100 xxxx */
	/* LDREXH		1110 1000 1101 xxxx xxxx xxxx 0101 xxxx */
	/* LDREXD		1110 1000 1101 xxxx xxxx xxxx 0111 xxxx */
	/* And unallocated instructions...				*/
	DECODE_END
};

static const union decode_item t32_table_1110_101x[] = {
	/* Data-processing (shifted register)				*/

	/* TST			1110 1010 0001 xxxx xxxx 1111 xxxx xxxx */
	/* TEQ			1110 1010 1001 xxxx xxxx 1111 xxxx xxxx */
	DECODE_EMULATEX	(0xff700f00, 0xea100f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, 0, 0, NOSPPC)),

	/* CMN			1110 1011 0001 xxxx xxxx 1111 xxxx xxxx */
	DECODE_OR	(0xfff00f00, 0xeb100f00),
	/* CMP			1110 1011 1011 xxxx xxxx 1111 xxxx xxxx */
	DECODE_EMULATEX	(0xfff00f00, 0xebb00f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOPC, 0, 0, 0, NOSPPC)),

	/* MOV			1110 1010 010x 1111 xxxx xxxx xxxx xxxx */
	/* MVN			1110 1010 011x 1111 xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xffcf0000, 0xea4f0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, NOSPPC)),

	/* ???			1110 1010 101x xxxx xxxx xxxx xxxx xxxx */
	/* ???			1110 1010 111x xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffa00000, 0xeaa00000),
	/* ???			1110 1011 001x xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffe00000, 0xeb200000),
	/* ???			1110 1011 100x xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffe00000, 0xeb800000),
	/* ???			1110 1011 111x xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xffe00000, 0xebe00000),

	/* ADD/SUB SP, SP, Rm, LSL #0..3				*/
	/*			1110 1011 x0xx 1101 x000 1101 xx00 xxxx */
	DECODE_EMULATEX	(0xff4f7f30, 0xeb0d0d00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, SP, 0, NOSPPC)),

	/* ADD/SUB SP, SP, Rm, shift					*/
	/*			1110 1011 x0xx 1101 xxxx 1101 xxxx xxxx */
	DECODE_REJECT	(0xff4f0f00, 0xeb0d0d00),

	/* ADD/SUB Rd, SP, Rm, shift					*/
	/*			1110 1011 x0xx 1101 xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xff4f0000, 0xeb0d0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, NOPC, 0, NOSPPC)),

	/* AND			1110 1010 000x xxxx xxxx xxxx xxxx xxxx */
	/* BIC			1110 1010 001x xxxx xxxx xxxx xxxx xxxx */
	/* ORR			1110 1010 010x xxxx xxxx xxxx xxxx xxxx */
	/* ORN			1110 1010 011x xxxx xxxx xxxx xxxx xxxx */
	/* EOR			1110 1010 100x xxxx xxxx xxxx xxxx xxxx */
	/* PKH			1110 1010 110x xxxx xxxx xxxx xxxx xxxx */
	/* ADD			1110 1011 000x xxxx xxxx xxxx xxxx xxxx */
	/* ADC			1110 1011 010x xxxx xxxx xxxx xxxx xxxx */
	/* SBC			1110 1011 011x xxxx xxxx xxxx xxxx xxxx */
	/* SUB			1110 1011 101x xxxx xxxx xxxx xxxx xxxx */
	/* RSB			1110 1011 110x xxxx xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfe000000, 0xea000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, NOSPPC)),

	DECODE_END
};

static const union decode_item t32_table_1111_0x0x___0[] = {
	/* Data-processing (modified immediate)				*/

	/* TST			1111 0x00 0001 xxxx 0xxx 1111 xxxx xxxx */
	/* TEQ			1111 0x00 1001 xxxx 0xxx 1111 xxxx xxxx */
	DECODE_EMULATEX	(0xfb708f00, 0xf0100f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, 0, 0, 0)),

	/* CMN			1111 0x01 0001 xxxx 0xxx 1111 xxxx xxxx */
	DECODE_OR	(0xfbf08f00, 0xf1100f00),
	/* CMP			1111 0x01 1011 xxxx 0xxx 1111 xxxx xxxx */
	DECODE_EMULATEX	(0xfbf08f00, 0xf1b00f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOPC, 0, 0, 0, 0)),

	/* MOV			1111 0x00 010x 1111 0xxx xxxx xxxx xxxx */
	/* MVN			1111 0x00 011x 1111 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfbcf8000, 0xf04f0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	/* ???			1111 0x00 101x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfbe08000, 0xf0a00000),
	/* ???			1111 0x00 110x xxxx 0xxx xxxx xxxx xxxx */
	/* ???			1111 0x00 111x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfbc08000, 0xf0c00000),
	/* ???			1111 0x01 001x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfbe08000, 0xf1200000),
	/* ???			1111 0x01 100x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfbe08000, 0xf1800000),
	/* ???			1111 0x01 111x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfbe08000, 0xf1e00000),

	/* ADD Rd, SP, #imm	1111 0x01 000x 1101 0xxx xxxx xxxx xxxx */
	/* SUB Rd, SP, #imm	1111 0x01 101x 1101 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfb4f8000, 0xf10d0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, NOPC, 0, 0)),

	/* AND			1111 0x00 000x xxxx 0xxx xxxx xxxx xxxx */
	/* BIC			1111 0x00 001x xxxx 0xxx xxxx xxxx xxxx */
	/* ORR			1111 0x00 010x xxxx 0xxx xxxx xxxx xxxx */
	/* ORN			1111 0x00 011x xxxx 0xxx xxxx xxxx xxxx */
	/* EOR			1111 0x00 100x xxxx 0xxx xxxx xxxx xxxx */
	/* ADD			1111 0x01 000x xxxx 0xxx xxxx xxxx xxxx */
	/* ADC			1111 0x01 010x xxxx 0xxx xxxx xxxx xxxx */
	/* SBC			1111 0x01 011x xxxx 0xxx xxxx xxxx xxxx */
	/* SUB			1111 0x01 101x xxxx 0xxx xxxx xxxx xxxx */
	/* RSB			1111 0x01 110x xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfa008000, 0xf0000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	DECODE_END
};

static const union decode_item t32_table_1111_0x1x___0[] = {
	/* Data-processing (plain binary immediate)			*/

	/* ADDW Rd, PC, #imm	1111 0x10 0000 1111 0xxx xxxx xxxx xxxx */
	DECODE_OR	(0xfbff8000, 0xf20f0000),
	/* SUBW	Rd, PC, #imm	1111 0x10 1010 1111 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfbff8000, 0xf2af0000, t32_emulate_rd8pc16_noflags,
						 REGS(PC, 0, NOSPPC, 0, 0)),

	/* ADDW SP, SP, #imm	1111 0x10 0000 1101 0xxx 1101 xxxx xxxx */
	DECODE_OR	(0xfbff8f00, 0xf20d0d00),
	/* SUBW	SP, SP, #imm	1111 0x10 1010 1101 0xxx 1101 xxxx xxxx */
	DECODE_EMULATEX	(0xfbff8f00, 0xf2ad0d00, t32_emulate_rd8rn16_noflags,
						 REGS(SP, 0, SP, 0, 0)),

	/* ADDW			1111 0x10 0000 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_OR	(0xfbf08000, 0xf2000000),
	/* SUBW			1111 0x10 1010 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfbf08000, 0xf2a00000, t32_emulate_rd8rn16_noflags,
						 REGS(NOPCX, 0, NOSPPC, 0, 0)),

	/* MOVW			1111 0x10 0100 xxxx 0xxx xxxx xxxx xxxx */
	/* MOVT			1111 0x10 1100 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfb708000, 0xf2400000, t32_emulate_rd8rn16_noflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	/* SSAT16		1111 0x11 0010 xxxx 0000 xxxx 00xx xxxx */
	/* SSAT			1111 0x11 00x0 xxxx 0xxx xxxx xxxx xxxx */
	/* USAT16		1111 0x11 1010 xxxx 0000 xxxx 00xx xxxx */
	/* USAT			1111 0x11 10x0 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfb508000, 0xf3000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	/* SFBX			1111 0x11 0100 xxxx 0xxx xxxx xxxx xxxx */
	/* UFBX			1111 0x11 1100 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfb708000, 0xf3400000, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	/* BFC			1111 0x11 0110 1111 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfbff8000, 0xf36f0000, t32_emulate_rd8rn16_noflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	/* BFI			1111 0x11 0110 xxxx 0xxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfbf08000, 0xf3600000, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPCX, 0, NOSPPC, 0, 0)),

	DECODE_END
};

static const union decode_item t32_table_1111_0xxx___1[] = {
	/* Branches and miscellaneous control				*/

	/* YIELD		1111 0011 1010 xxxx 10x0 x000 0000 0001 */
	DECODE_OR	(0xfff0d7ff, 0xf3a08001),
	/* SEV			1111 0011 1010 xxxx 10x0 x000 0000 0100 */
	DECODE_EMULATE	(0xfff0d7ff, 0xf3a08004, kprobe_emulate_none),
	/* NOP			1111 0011 1010 xxxx 10x0 x000 0000 0000 */
	/* WFE			1111 0011 1010 xxxx 10x0 x000 0000 0010 */
	/* WFI			1111 0011 1010 xxxx 10x0 x000 0000 0011 */
	DECODE_SIMULATE	(0xfff0d7fc, 0xf3a08000, kprobe_simulate_nop),

	/* MRS Rd, CPSR		1111 0011 1110 xxxx 10x0 xxxx xxxx xxxx */
	DECODE_SIMULATEX(0xfff0d000, 0xf3e08000, t32_simulate_mrs,
						 REGS(0, 0, NOSPPC, 0, 0)),

	/*
	 * Unsupported instructions
	 *			1111 0x11 1xxx xxxx 10x0 xxxx xxxx xxxx
	 *
	 * MSR			1111 0011 100x xxxx 10x0 xxxx xxxx xxxx
	 * DBG hint		1111 0011 1010 xxxx 10x0 x000 1111 xxxx
	 * Unallocated hints	1111 0011 1010 xxxx 10x0 x000 xxxx xxxx
	 * CPS			1111 0011 1010 xxxx 10x0 xxxx xxxx xxxx
	 * CLREX/DSB/DMB/ISB	1111 0011 1011 xxxx 10x0 xxxx xxxx xxxx
	 * BXJ			1111 0011 1100 xxxx 10x0 xxxx xxxx xxxx
	 * SUBS PC,LR,#<imm8>	1111 0011 1101 xxxx 10x0 xxxx xxxx xxxx
	 * MRS Rd, SPSR		1111 0011 1111 xxxx 10x0 xxxx xxxx xxxx
	 * SMC			1111 0111 1111 xxxx 1000 xxxx xxxx xxxx
	 * UNDEFINED		1111 0111 1111 xxxx 1010 xxxx xxxx xxxx
	 * ???			1111 0111 1xxx xxxx 1010 xxxx xxxx xxxx
	 */
	DECODE_REJECT	(0xfb80d000, 0xf3808000),

	/* Bcc			1111 0xxx xxxx xxxx 10x0 xxxx xxxx xxxx */
	DECODE_CUSTOM	(0xf800d000, 0xf0008000, t32_decode_cond_branch),

	/* BLX			1111 0xxx xxxx xxxx 11x0 xxxx xxxx xxx0 */
	DECODE_OR	(0xf800d001, 0xf000c000),
	/* B			1111 0xxx xxxx xxxx 10x1 xxxx xxxx xxxx */
	/* BL			1111 0xxx xxxx xxxx 11x1 xxxx xxxx xxxx */
	DECODE_SIMULATE	(0xf8009000, 0xf0009000, t32_simulate_branch),

	DECODE_END
};

static const union decode_item t32_table_1111_100x_x0x1__1111[] = {
	/* Memory hints							*/

	/* PLD (literal)	1111 1000 x001 1111 1111 xxxx xxxx xxxx */
	/* PLI (literal)	1111 1001 x001 1111 1111 xxxx xxxx xxxx */
	DECODE_SIMULATE	(0xfe7ff000, 0xf81ff000, kprobe_simulate_nop),

	/* PLD{W} (immediate)	1111 1000 10x1 xxxx 1111 xxxx xxxx xxxx */
	DECODE_OR	(0xffd0f000, 0xf890f000),
	/* PLD{W} (immediate)	1111 1000 00x1 xxxx 1111 1100 xxxx xxxx */
	DECODE_OR	(0xffd0ff00, 0xf810fc00),
	/* PLI (immediate)	1111 1001 1001 xxxx 1111 xxxx xxxx xxxx */
	DECODE_OR	(0xfff0f000, 0xf990f000),
	/* PLI (immediate)	1111 1001 0001 xxxx 1111 1100 xxxx xxxx */
	DECODE_SIMULATEX(0xfff0ff00, 0xf910fc00, kprobe_simulate_nop,
						 REGS(NOPCX, 0, 0, 0, 0)),

	/* PLD{W} (register)	1111 1000 00x1 xxxx 1111 0000 00xx xxxx */
	DECODE_OR	(0xffd0ffc0, 0xf810f000),
	/* PLI (register)	1111 1001 0001 xxxx 1111 0000 00xx xxxx */
	DECODE_SIMULATEX(0xfff0ffc0, 0xf910f000, kprobe_simulate_nop,
						 REGS(NOPCX, 0, 0, 0, NOSPPC)),

	/* Other unallocated instructions...				*/
	DECODE_END
};

static const union decode_item t32_table_1111_100x[] = {
	/* Store/Load single data item					*/

	/* ???			1111 100x x11x xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfe600000, 0xf8600000),

	/* ???			1111 1001 0101 xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xfff00000, 0xf9500000),

	/* ???			1111 100x 0xxx xxxx xxxx 10x0 xxxx xxxx */
	DECODE_REJECT	(0xfe800d00, 0xf8000800),

	/* STRBT		1111 1000 0000 xxxx xxxx 1110 xxxx xxxx */
	/* STRHT		1111 1000 0010 xxxx xxxx 1110 xxxx xxxx */
	/* STRT			1111 1000 0100 xxxx xxxx 1110 xxxx xxxx */
	/* LDRBT		1111 1000 0001 xxxx xxxx 1110 xxxx xxxx */
	/* LDRSBT		1111 1001 0001 xxxx xxxx 1110 xxxx xxxx */
	/* LDRHT		1111 1000 0011 xxxx xxxx 1110 xxxx xxxx */
	/* LDRSHT		1111 1001 0011 xxxx xxxx 1110 xxxx xxxx */
	/* LDRT			1111 1000 0101 xxxx xxxx 1110 xxxx xxxx */
	DECODE_REJECT	(0xfe800f00, 0xf8000e00),

	/* STR{,B,H} Rn,[PC...]	1111 1000 xxx0 1111 xxxx xxxx xxxx xxxx */
	DECODE_REJECT	(0xff1f0000, 0xf80f0000),

	/* STR{,B,H} PC,[Rn...]	1111 1000 xxx0 xxxx 1111 xxxx xxxx xxxx */
	DECODE_REJECT	(0xff10f000, 0xf800f000),

	/* LDR (literal)	1111 1000 x101 1111 xxxx xxxx xxxx xxxx */
	DECODE_SIMULATEX(0xff7f0000, 0xf85f0000, t32_simulate_ldr_literal,
						 REGS(PC, ANY, 0, 0, 0)),

	/* STR (immediate)	1111 1000 0100 xxxx xxxx 1xxx xxxx xxxx */
	/* LDR (immediate)	1111 1000 0101 xxxx xxxx 1xxx xxxx xxxx */
	DECODE_OR	(0xffe00800, 0xf8400800),
	/* STR (immediate)	1111 1000 1100 xxxx xxxx xxxx xxxx xxxx */
	/* LDR (immediate)	1111 1000 1101 xxxx xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xffe00000, 0xf8c00000, t32_emulate_ldrstr,
						 REGS(NOPCX, ANY, 0, 0, 0)),

	/* STR (register)	1111 1000 0100 xxxx xxxx 0000 00xx xxxx */
	/* LDR (register)	1111 1000 0101 xxxx xxxx 0000 00xx xxxx */
	DECODE_EMULATEX	(0xffe00fc0, 0xf8400000, t32_emulate_ldrstr,
						 REGS(NOPCX, ANY, 0, 0, NOSPPC)),

	/* LDRB (literal)	1111 1000 x001 1111 xxxx xxxx xxxx xxxx */
	/* LDRSB (literal)	1111 1001 x001 1111 xxxx xxxx xxxx xxxx */
	/* LDRH (literal)	1111 1000 x011 1111 xxxx xxxx xxxx xxxx */
	/* LDRSH (literal)	1111 1001 x011 1111 xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfe5f0000, 0xf81f0000, t32_simulate_ldr_literal,
						 REGS(PC, NOSPPCX, 0, 0, 0)),

	/* STRB (immediate)	1111 1000 0000 xxxx xxxx 1xxx xxxx xxxx */
	/* STRH (immediate)	1111 1000 0010 xxxx xxxx 1xxx xxxx xxxx */
	/* LDRB (immediate)	1111 1000 0001 xxxx xxxx 1xxx xxxx xxxx */
	/* LDRSB (immediate)	1111 1001 0001 xxxx xxxx 1xxx xxxx xxxx */
	/* LDRH (immediate)	1111 1000 0011 xxxx xxxx 1xxx xxxx xxxx */
	/* LDRSH (immediate)	1111 1001 0011 xxxx xxxx 1xxx xxxx xxxx */
	DECODE_OR	(0xfec00800, 0xf8000800),
	/* STRB (immediate)	1111 1000 1000 xxxx xxxx xxxx xxxx xxxx */
	/* STRH (immediate)	1111 1000 1010 xxxx xxxx xxxx xxxx xxxx */
	/* LDRB (immediate)	1111 1000 1001 xxxx xxxx xxxx xxxx xxxx */
	/* LDRSB (immediate)	1111 1001 1001 xxxx xxxx xxxx xxxx xxxx */
	/* LDRH (immediate)	1111 1000 1011 xxxx xxxx xxxx xxxx xxxx */
	/* LDRSH (immediate)	1111 1001 1011 xxxx xxxx xxxx xxxx xxxx */
	DECODE_EMULATEX	(0xfec00000, 0xf8800000, t32_emulate_ldrstr,
						 REGS(NOPCX, NOSPPCX, 0, 0, 0)),

	/* STRB (register)	1111 1000 0000 xxxx xxxx 0000 00xx xxxx */
	/* STRH (register)	1111 1000 0010 xxxx xxxx 0000 00xx xxxx */
	/* LDRB (register)	1111 1000 0001 xxxx xxxx 0000 00xx xxxx */
	/* LDRSB (register)	1111 1001 0001 xxxx xxxx 0000 00xx xxxx */
	/* LDRH (register)	1111 1000 0011 xxxx xxxx 0000 00xx xxxx */
	/* LDRSH (register)	1111 1001 0011 xxxx xxxx 0000 00xx xxxx */
	DECODE_EMULATEX	(0xfe800fc0, 0xf8000000, t32_emulate_ldrstr,
						 REGS(NOPCX, NOSPPCX, 0, 0, NOSPPC)),

	/* Other unallocated instructions...				*/
	DECODE_END
};

static const union decode_item t32_table_1111_1010___1111[] = {
	/* Data-processing (register)					*/

	/* ???			1111 1010 011x xxxx 1111 xxxx 1xxx xxxx */
	DECODE_REJECT	(0xffe0f080, 0xfa60f080),

	/* SXTH			1111 1010 0000 1111 1111 xxxx 1xxx xxxx */
	/* UXTH			1111 1010 0001 1111 1111 xxxx 1xxx xxxx */
	/* SXTB16		1111 1010 0010 1111 1111 xxxx 1xxx xxxx */
	/* UXTB16		1111 1010 0011 1111 1111 xxxx 1xxx xxxx */
	/* SXTB			1111 1010 0100 1111 1111 xxxx 1xxx xxxx */
	/* UXTB			1111 1010 0101 1111 1111 xxxx 1xxx xxxx */
	DECODE_EMULATEX	(0xff8ff080, 0xfa0ff080, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, NOSPPC)),


	/* ???			1111 1010 1xxx xxxx 1111 xxxx 0x11 xxxx */
	DECODE_REJECT	(0xff80f0b0, 0xfa80f030),
	/* ???			1111 1010 1x11 xxxx 1111 xxxx 0xxx xxxx */
	DECODE_REJECT	(0xffb0f080, 0xfab0f000),

	/* SADD16		1111 1010 1001 xxxx 1111 xxxx 0000 xxxx */
	/* SASX			1111 1010 1010 xxxx 1111 xxxx 0000 xxxx */
	/* SSAX			1111 1010 1110 xxxx 1111 xxxx 0000 xxxx */
	/* SSUB16		1111 1010 1101 xxxx 1111 xxxx 0000 xxxx */
	/* SADD8		1111 1010 1000 xxxx 1111 xxxx 0000 xxxx */
	/* SSUB8		1111 1010 1100 xxxx 1111 xxxx 0000 xxxx */

	/* QADD16		1111 1010 1001 xxxx 1111 xxxx 0001 xxxx */
	/* QASX			1111 1010 1010 xxxx 1111 xxxx 0001 xxxx */
	/* QSAX			1111 1010 1110 xxxx 1111 xxxx 0001 xxxx */
	/* QSUB16		1111 1010 1101 xxxx 1111 xxxx 0001 xxxx */
	/* QADD8		1111 1010 1000 xxxx 1111 xxxx 0001 xxxx */
	/* QSUB8		1111 1010 1100 xxxx 1111 xxxx 0001 xxxx */

	/* SHADD16		1111 1010 1001 xxxx 1111 xxxx 0010 xxxx */
	/* SHASX		1111 1010 1010 xxxx 1111 xxxx 0010 xxxx */
	/* SHSAX		1111 1010 1110 xxxx 1111 xxxx 0010 xxxx */
	/* SHSUB16		1111 1010 1101 xxxx 1111 xxxx 0010 xxxx */
	/* SHADD8		1111 1010 1000 xxxx 1111 xxxx 0010 xxxx */
	/* SHSUB8		1111 1010 1100 xxxx 1111 xxxx 0010 xxxx */

	/* UADD16		1111 1010 1001 xxxx 1111 xxxx 0100 xxxx */
	/* UASX			1111 1010 1010 xxxx 1111 xxxx 0100 xxxx */
	/* USAX			1111 1010 1110 xxxx 1111 xxxx 0100 xxxx */
	/* USUB16		1111 1010 1101 xxxx 1111 xxxx 0100 xxxx */
	/* UADD8		1111 1010 1000 xxxx 1111 xxxx 0100 xxxx */
	/* USUB8		1111 1010 1100 xxxx 1111 xxxx 0100 xxxx */

	/* UQADD16		1111 1010 1001 xxxx 1111 xxxx 0101 xxxx */
	/* UQASX		1111 1010 1010 xxxx 1111 xxxx 0101 xxxx */
	/* UQSAX		1111 1010 1110 xxxx 1111 xxxx 0101 xxxx */
	/* UQSUB16		1111 1010 1101 xxxx 1111 xxxx 0101 xxxx */
	/* UQADD8		1111 1010 1000 xxxx 1111 xxxx 0101 xxxx */
	/* UQSUB8		1111 1010 1100 xxxx 1111 xxxx 0101 xxxx */

	/* UHADD16		1111 1010 1001 xxxx 1111 xxxx 0110 xxxx */
	/* UHASX		1111 1010 1010 xxxx 1111 xxxx 0110 xxxx */
	/* UHSAX		1111 1010 1110 xxxx 1111 xxxx 0110 xxxx */
	/* UHSUB16		1111 1010 1101 xxxx 1111 xxxx 0110 xxxx */
	/* UHADD8		1111 1010 1000 xxxx 1111 xxxx 0110 xxxx */
	/* UHSUB8		1111 1010 1100 xxxx 1111 xxxx 0110 xxxx */
	DECODE_OR	(0xff80f080, 0xfa80f000),

	/* SXTAH		1111 1010 0000 xxxx 1111 xxxx 1xxx xxxx */
	/* UXTAH		1111 1010 0001 xxxx 1111 xxxx 1xxx xxxx */
	/* SXTAB16		1111 1010 0010 xxxx 1111 xxxx 1xxx xxxx */
	/* UXTAB16		1111 1010 0011 xxxx 1111 xxxx 1xxx xxxx */
	/* SXTAB		1111 1010 0100 xxxx 1111 xxxx 1xxx xxxx */
	/* UXTAB		1111 1010 0101 xxxx 1111 xxxx 1xxx xxxx */
	DECODE_OR	(0xff80f080, 0xfa00f080),

	/* QADD			1111 1010 1000 xxxx 1111 xxxx 1000 xxxx */
	/* QDADD		1111 1010 1000 xxxx 1111 xxxx 1001 xxxx */
	/* QSUB			1111 1010 1000 xxxx 1111 xxxx 1010 xxxx */
	/* QDSUB		1111 1010 1000 xxxx 1111 xxxx 1011 xxxx */
	DECODE_OR	(0xfff0f0c0, 0xfa80f080),

	/* SEL			1111 1010 1010 xxxx 1111 xxxx 1000 xxxx */
	DECODE_OR	(0xfff0f0f0, 0xfaa0f080),

	/* LSL			1111 1010 000x xxxx 1111 xxxx 0000 xxxx */
	/* LSR			1111 1010 001x xxxx 1111 xxxx 0000 xxxx */
	/* ASR			1111 1010 010x xxxx 1111 xxxx 0000 xxxx */
	/* ROR			1111 1010 011x xxxx 1111 xxxx 0000 xxxx */
	DECODE_EMULATEX	(0xff80f0f0, 0xfa00f000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, NOSPPC)),

	/* CLZ			1111 1010 1010 xxxx 1111 xxxx 1000 xxxx */
	DECODE_OR	(0xfff0f0f0, 0xfab0f080),

	/* REV			1111 1010 1001 xxxx 1111 xxxx 1000 xxxx */
	/* REV16		1111 1010 1001 xxxx 1111 xxxx 1001 xxxx */
	/* RBIT			1111 1010 1001 xxxx 1111 xxxx 1010 xxxx */
	/* REVSH		1111 1010 1001 xxxx 1111 xxxx 1011 xxxx */
	DECODE_EMULATEX	(0xfff0f0c0, 0xfa90f080, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, SAMEAS16)),

	/* Other unallocated instructions...				*/
	DECODE_END
};

const union decode_item kprobe_decode_thumb32_table[] = {

	/*
	 * Load/store multiple instructions
	 *			1110 100x x0xx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfe400000, 0xe8000000, t32_table_1110_100x_x0xx),

	/*
	 * Load/store dual, load/store exclusive, table branch
	 *			1110 100x x1xx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfe400000, 0xe8400000, t32_table_1110_100x_x1xx),

	/*
	 * Data-processing (shifted register)
	 *			1110 101x xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfe000000, 0xea000000, t32_table_1110_101x),

	/*
	 * Coprocessor instructions
	 *			1110 11xx xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_REJECT	(0xfc000000, 0xec000000),

	/*
	 * Data-processing (modified immediate)
	 *			1111 0x0x xxxx xxxx 0xxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfa008000, 0xf0000000, t32_table_1111_0x0x___0),

	/*
	 * Data-processing (plain binary immediate)
	 *			1111 0x1x xxxx xxxx 0xxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfa008000, 0xf2000000, t32_table_1111_0x1x___0),

	/*
	 * Branches and miscellaneous control
	 *			1111 0xxx xxxx xxxx 1xxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xf8008000, 0xf0008000, t32_table_1111_0xxx___1),

	/*
	 * Advanced SIMD element or structure load/store instructions
	 *			1111 1001 xxx0 xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_REJECT	(0xff100000, 0xf9000000),

	/*
	 * Memory hints
	 *			1111 100x x0x1 xxxx 1111 xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfe50f000, 0xf810f000, t32_table_1111_100x_x0x1__1111),

	/*
	 * Store single data item
	 *			1111 1000 xxx0 xxxx xxxx xxxx xxxx xxxx
	 * Load single data items
	 *			1111 100x xxx1 xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xfe000000, 0xf8000000, t32_table_1111_100x),

	/*
	 * Data-processing (register)
	 *			1111 1010 xxxx xxxx 1111 xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xff00f000, 0xfa00f000, t32_table_1111_1010___1111),

	/*
	 * Coprocessor instructions
	 *			1111 11xx xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_END
};

static void __kprobes
t16_simulate_bxblx(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rm = (insn >> 3) & 0xf;
	unsigned long rmv = (rm == 15) ? pc : regs->uregs[rm];

	if (insn & (1 << 7)) /* BLX ? */
		regs->ARM_lr = (unsigned long)p->addr + 2;

	bx_write_pc(rmv, regs);
}

static void __kprobes
t16_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long* base = (unsigned long *)(thumb_probe_pc(p) & ~3);
	long index = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	regs->uregs[rt] = base[index];
}

static void __kprobes
t16_simulate_ldrstr_sp_relative(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long* base = (unsigned long *)regs->ARM_sp;
	long index = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	if (insn & 0x800) /* LDR */
		regs->uregs[rt] = base[index];
	else /* STR */
		base[index] = regs->uregs[rt];
}

static void __kprobes
t16_simulate_reladr(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long base = (insn & 0x800) ? regs->ARM_sp
					    : (thumb_probe_pc(p) & ~3);
	long offset = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	regs->uregs[rt] = base + offset * 4;
}

static void __kprobes
t16_simulate_add_sp_imm(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	long imm = insn & 0x7f;
	if (insn & 0x80) /* SUB */
		regs->ARM_sp -= imm * 4;
	else /* ADD */
		regs->ARM_sp += imm * 4;
}

static void __kprobes
t16_simulate_cbz(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rn = insn & 0x7;
	kprobe_opcode_t nonzero = regs->uregs[rn] ? insn : ~insn;
	if (nonzero & 0x800) {
		long i = insn & 0x200;
		long imm5 = insn & 0xf8;
		unsigned long pc = thumb_probe_pc(p);
		regs->ARM_pc = pc + (i >> 3) + (imm5 >> 2);
	}
}

static void __kprobes
t16_simulate_it(struct kprobe *p, struct pt_regs *regs)
{
	/*
	 * The 8 IT state bits are split into two parts in CPSR:
	 *	ITSTATE<1:0> are in CPSR<26:25>
	 *	ITSTATE<7:2> are in CPSR<15:10>
	 * The new IT state is in the lower byte of insn.
	 */
	kprobe_opcode_t insn = p->opcode;
	unsigned long cpsr = regs->ARM_cpsr;
	cpsr &= ~PSR_IT_MASK;
	cpsr |= (insn & 0xfc) << 8;
	cpsr |= (insn & 0x03) << 25;
	regs->ARM_cpsr = cpsr;
}

static void __kprobes
t16_singlestep_it(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 2;
	t16_simulate_it(p, regs);
}

static enum kprobe_insn __kprobes
t16_decode_it(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = t16_singlestep_it;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t16_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	long offset = insn & 0x7f;
	offset -= insn & 0x80; /* Apply sign bit */
	regs->ARM_pc = pc + (offset * 2);
}

static enum kprobe_insn __kprobes
t16_decode_cond_branch(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	int cc = (insn >> 8) & 0xf;
	asi->insn_check_cc = kprobe_condition_checks[cc];
	asi->insn_handler = t16_simulate_cond_branch;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t16_simulate_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	long offset = insn & 0x3ff;
	offset -= insn & 0x400; /* Apply sign bit */
	regs->ARM_pc = pc + (offset * 2);
}

static unsigned long __kprobes
t16_emulate_loregs(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long oldcpsr = regs->ARM_cpsr;
	unsigned long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: [newcpsr] "=r" (newcpsr)
		: [oldcpsr] "r" (oldcpsr), [regs] "r" (regs),
		  [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
		  "lr", "memory", "cc"
		);

	return (oldcpsr & ~APSR_MASK) | (newcpsr & APSR_MASK);
}

static void __kprobes
t16_emulate_loregs_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_cpsr = t16_emulate_loregs(p, regs);
}

static void __kprobes
t16_emulate_loregs_noitrwflags(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long cpsr = t16_emulate_loregs(p, regs);
	if (!in_it_block(cpsr))
		regs->ARM_cpsr = cpsr;
}

static void __kprobes
t16_emulate_hiregs(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rdn = (insn & 0x7) | ((insn & 0x80) >> 4);
	int rm = (insn >> 3) & 0xf;

	register unsigned long rdnv asm("r1");
	register unsigned long rmv asm("r0");
	unsigned long cpsr = regs->ARM_cpsr;

	rdnv = (rdn == 15) ? pc : regs->uregs[rdn];
	rmv = (rm == 15) ? pc : regs->uregs[rm];

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"blx    %[fn]			\n\t"
		"mrs	%[cpsr], cpsr		\n\t"
		: "=r" (rdnv), [cpsr] "=r" (cpsr)
		: "0" (rdnv), "r" (rmv), "1" (cpsr), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	if (rdn == 15)
		rdnv &= ~1;

	regs->uregs[rdn] = rdnv;
	regs->ARM_cpsr = (regs->ARM_cpsr & ~APSR_MASK) | (cpsr & APSR_MASK);
}

static enum kprobe_insn __kprobes
t16_decode_hiregs(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= ~0x00ff;
	insn |= 0x001; /* Set Rdn = R1 and Rm = R0 */
	((u16 *)asi->insn)[0] = insn;
	asi->insn_handler = t16_emulate_hiregs;
	return INSN_GOOD;
}

static void __kprobes
t16_emulate_push(struct kprobe *p, struct pt_regs *regs)
{
	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldr	r8, [%[regs], #14*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		:
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
		  "lr", "memory", "cc"
		);
}

static enum kprobe_insn __kprobes
t16_decode_push(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	/*
	 * To simulate a PUSH we use a Thumb-2 "STMDB R9!, {registers}"
	 * and call it with R9=SP and LR in the register list represented
	 * by R8.
	 */
	((u16 *)asi->insn)[0] = 0xe929;		/* 1st half STMDB R9!,{} */
	((u16 *)asi->insn)[1] = insn & 0x1ff;	/* 2nd half (register list) */
	asi->insn_handler = t16_emulate_push;
	return INSN_GOOD;
}

static void __kprobes
t16_emulate_pop_nopc(struct kprobe *p, struct pt_regs *regs)
{
	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		:
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r9",
		  "lr", "memory", "cc"
		);
}

static void __kprobes
t16_emulate_pop_pc(struct kprobe *p, struct pt_regs *regs)
{
	register unsigned long pc asm("r8");

	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		: "=r" (pc)
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r9",
		  "lr", "memory", "cc"
		);

	bx_write_pc(pc, regs);
}

static enum kprobe_insn __kprobes
t16_decode_pop(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	/*
	 * To simulate a POP we use a Thumb-2 "LDMDB R9!, {registers}"
	 * and call it with R9=SP and PC in the register list represented
	 * by R8.
	 */
	((u16 *)asi->insn)[0] = 0xe8b9;		/* 1st half LDMIA R9!,{} */
	((u16 *)asi->insn)[1] = insn & 0x1ff;	/* 2nd half (register list) */
	asi->insn_handler = insn & 0x100 ? t16_emulate_pop_pc
					 : t16_emulate_pop_nopc;
	return INSN_GOOD;
}

static const union decode_item t16_table_1011[] = {
	/* Miscellaneous 16-bit instructions		    */

	/* ADD (SP plus immediate)	1011 0000 0xxx xxxx */
	/* SUB (SP minus immediate)	1011 0000 1xxx xxxx */
	DECODE_SIMULATE	(0xff00, 0xb000, t16_simulate_add_sp_imm),

	/* CBZ				1011 00x1 xxxx xxxx */
	/* CBNZ				1011 10x1 xxxx xxxx */
	DECODE_SIMULATE	(0xf500, 0xb100, t16_simulate_cbz),

	/* SXTH				1011 0010 00xx xxxx */
	/* SXTB				1011 0010 01xx xxxx */
	/* UXTH				1011 0010 10xx xxxx */
	/* UXTB				1011 0010 11xx xxxx */
	/* REV				1011 1010 00xx xxxx */
	/* REV16			1011 1010 01xx xxxx */
	/* ???				1011 1010 10xx xxxx */
	/* REVSH			1011 1010 11xx xxxx */
	DECODE_REJECT	(0xffc0, 0xba80),
	DECODE_EMULATE	(0xf500, 0xb000, t16_emulate_loregs_rwflags),

	/* PUSH				1011 010x xxxx xxxx */
	DECODE_CUSTOM	(0xfe00, 0xb400, t16_decode_push),
	/* POP				1011 110x xxxx xxxx */
	DECODE_CUSTOM	(0xfe00, 0xbc00, t16_decode_pop),

	/*
	 * If-Then, and hints
	 *				1011 1111 xxxx xxxx
	 */

	/* YIELD			1011 1111 0001 0000 */
	DECODE_OR	(0xffff, 0xbf10),
	/* SEV				1011 1111 0100 0000 */
	DECODE_EMULATE	(0xffff, 0xbf40, kprobe_emulate_none),
	/* NOP				1011 1111 0000 0000 */
	/* WFE				1011 1111 0010 0000 */
	/* WFI				1011 1111 0011 0000 */
	DECODE_SIMULATE	(0xffcf, 0xbf00, kprobe_simulate_nop),
	/* Unassigned hints		1011 1111 xxxx 0000 */
	DECODE_REJECT	(0xff0f, 0xbf00),
	/* IT				1011 1111 xxxx xxxx */
	DECODE_CUSTOM	(0xff00, 0xbf00, t16_decode_it),

	/* SETEND			1011 0110 010x xxxx */
	/* CPS				1011 0110 011x xxxx */
	/* BKPT				1011 1110 xxxx xxxx */
	/* And unallocated instructions...		    */
	DECODE_END
};

const union decode_item kprobe_decode_thumb16_table[] = {

	/*
	 * Shift (immediate), add, subtract, move, and compare
	 *				00xx xxxx xxxx xxxx
	 */

	/* CMP (immediate)		0010 1xxx xxxx xxxx */
	DECODE_EMULATE	(0xf800, 0x2800, t16_emulate_loregs_rwflags),

	/* ADD (register)		0001 100x xxxx xxxx */
	/* SUB (register)		0001 101x xxxx xxxx */
	/* LSL (immediate)		0000 0xxx xxxx xxxx */
	/* LSR (immediate)		0000 1xxx xxxx xxxx */
	/* ASR (immediate)		0001 0xxx xxxx xxxx */
	/* ADD (immediate, Thumb)	0001 110x xxxx xxxx */
	/* SUB (immediate, Thumb)	0001 111x xxxx xxxx */
	/* MOV (immediate)		0010 0xxx xxxx xxxx */
	/* ADD (immediate, Thumb)	0011 0xxx xxxx xxxx */
	/* SUB (immediate, Thumb)	0011 1xxx xxxx xxxx */
	DECODE_EMULATE	(0xc000, 0x0000, t16_emulate_loregs_noitrwflags),

	/*
	 * 16-bit Thumb data-processing instructions
	 *				0100 00xx xxxx xxxx
	 */

	/* TST (register)		0100 0010 00xx xxxx */
	DECODE_EMULATE	(0xffc0, 0x4200, t16_emulate_loregs_rwflags),
	/* CMP (register)		0100 0010 10xx xxxx */
	/* CMN (register)		0100 0010 11xx xxxx */
	DECODE_EMULATE	(0xff80, 0x4280, t16_emulate_loregs_rwflags),
	/* AND (register)		0100 0000 00xx xxxx */
	/* EOR (register)		0100 0000 01xx xxxx */
	/* LSL (register)		0100 0000 10xx xxxx */
	/* LSR (register)		0100 0000 11xx xxxx */
	/* ASR (register)		0100 0001 00xx xxxx */
	/* ADC (register)		0100 0001 01xx xxxx */
	/* SBC (register)		0100 0001 10xx xxxx */
	/* ROR (register)		0100 0001 11xx xxxx */
	/* RSB (immediate)		0100 0010 01xx xxxx */
	/* ORR (register)		0100 0011 00xx xxxx */
	/* MUL				0100 0011 00xx xxxx */
	/* BIC (register)		0100 0011 10xx xxxx */
	/* MVN (register)		0100 0011 10xx xxxx */
	DECODE_EMULATE	(0xfc00, 0x4000, t16_emulate_loregs_noitrwflags),

	/*
	 * Special data instructions and branch and exchange
	 *				0100 01xx xxxx xxxx
	 */

	/* BLX pc			0100 0111 1111 1xxx */
	DECODE_REJECT	(0xfff8, 0x47f8),

	/* BX (register)		0100 0111 0xxx xxxx */
	/* BLX (register)		0100 0111 1xxx xxxx */
	DECODE_SIMULATE (0xff00, 0x4700, t16_simulate_bxblx),

	/* ADD pc, pc			0100 0100 1111 1111 */
	DECODE_REJECT	(0xffff, 0x44ff),

	/* ADD (register)		0100 0100 xxxx xxxx */
	/* CMP (register)		0100 0101 xxxx xxxx */
	/* MOV (register)		0100 0110 xxxx xxxx */
	DECODE_CUSTOM	(0xfc00, 0x4400, t16_decode_hiregs),

	/*
	 * Load from Literal Pool
	 * LDR (literal)		0100 1xxx xxxx xxxx
	 */
	DECODE_SIMULATE	(0xf800, 0x4800, t16_simulate_ldr_literal),

	/*
	 * 16-bit Thumb Load/store instructions
	 *				0101 xxxx xxxx xxxx
	 *				011x xxxx xxxx xxxx
	 *				100x xxxx xxxx xxxx
	 */

	/* STR (register)		0101 000x xxxx xxxx */
	/* STRH (register)		0101 001x xxxx xxxx */
	/* STRB (register)		0101 010x xxxx xxxx */
	/* LDRSB (register)		0101 011x xxxx xxxx */
	/* LDR (register)		0101 100x xxxx xxxx */
	/* LDRH (register)		0101 101x xxxx xxxx */
	/* LDRB (register)		0101 110x xxxx xxxx */
	/* LDRSH (register)		0101 111x xxxx xxxx */
	/* STR (immediate, Thumb)	0110 0xxx xxxx xxxx */
	/* LDR (immediate, Thumb)	0110 1xxx xxxx xxxx */
	/* STRB (immediate, Thumb)	0111 0xxx xxxx xxxx */
	/* LDRB (immediate, Thumb)	0111 1xxx xxxx xxxx */
	DECODE_EMULATE	(0xc000, 0x4000, t16_emulate_loregs_rwflags),
	/* STRH (immediate, Thumb)	1000 0xxx xxxx xxxx */
	/* LDRH (immediate, Thumb)	1000 1xxx xxxx xxxx */
	DECODE_EMULATE	(0xf000, 0x8000, t16_emulate_loregs_rwflags),
	/* STR (immediate, Thumb)	1001 0xxx xxxx xxxx */
	/* LDR (immediate, Thumb)	1001 1xxx xxxx xxxx */
	DECODE_SIMULATE	(0xf000, 0x9000, t16_simulate_ldrstr_sp_relative),

	/*
	 * Generate PC-/SP-relative address
	 * ADR (literal)		1010 0xxx xxxx xxxx
	 * ADD (SP plus immediate)	1010 1xxx xxxx xxxx
	 */
	DECODE_SIMULATE	(0xf000, 0xa000, t16_simulate_reladr),

	/*
	 * Miscellaneous 16-bit instructions
	 *				1011 xxxx xxxx xxxx
	 */
	DECODE_TABLE	(0xf000, 0xb000, t16_table_1011),

	/* STM				1100 0xxx xxxx xxxx */
	/* LDM				1100 1xxx xxxx xxxx */
	DECODE_EMULATE	(0xf000, 0xc000, t16_emulate_loregs_rwflags),

	/*
	 * Conditional branch, and Supervisor Call
	 */

	/* Permanently UNDEFINED	1101 1110 xxxx xxxx */
	/* SVC				1101 1111 xxxx xxxx */
	DECODE_REJECT	(0xfe00, 0xde00),

	/* Conditional branch		1101 xxxx xxxx xxxx */
	DECODE_CUSTOM	(0xf000, 0xd000, t16_decode_cond_branch),

	/*
	 * Unconditional branch
	 * B				1110 0xxx xxxx xxxx
	 */
	DECODE_SIMULATE	(0xf800, 0xe000, t16_simulate_branch),

	DECODE_END
};

static unsigned long __kprobes thumb_check_cc(unsigned long cpsr)
{
	if (unlikely(in_it_block(cpsr)))
		return kprobe_condition_checks[current_cond(cpsr)](cpsr);
	return true;
}

static void __kprobes thumb16_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 2;
	p->ainsn.insn_handler(p, regs);
	regs->ARM_cpsr = it_advance(regs->ARM_cpsr);
}

static void __kprobes thumb32_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 4;
	p->ainsn.insn_handler(p, regs);
	regs->ARM_cpsr = it_advance(regs->ARM_cpsr);
}

enum kprobe_insn __kprobes
thumb16_kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = thumb16_singlestep;
	asi->insn_check_cc = thumb_check_cc;
	return kprobe_decode_insn(insn, asi, kprobe_decode_thumb16_table, true);
}

enum kprobe_insn __kprobes
thumb32_kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = thumb32_singlestep;
	asi->insn_check_cc = thumb_check_cc;
	return kprobe_decode_insn(insn, asi, kprobe_decode_thumb32_table, true);
}
