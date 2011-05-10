/*
 * tlv320aic32x4.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _TLV320AIC32X4_H
#define _TLV320AIC32X4_H

/* tlv320aic32x4 register space (in decimal to match datasheet) */

#define AIC32X4_PAGE1		128

#define	AIC32X4_PSEL		0
#define	AIC32X4_RESET		1
#define	AIC32X4_CLKMUX		4
#define	AIC32X4_PLLPR		5
#define	AIC32X4_PLLJ		6
#define	AIC32X4_PLLDMSB		7
#define	AIC32X4_PLLDLSB		8
#define	AIC32X4_NDAC		11
#define	AIC32X4_MDAC		12
#define AIC32X4_DOSRMSB		13
#define AIC32X4_DOSRLSB		14
#define	AIC32X4_NADC		18
#define	AIC32X4_MADC		19
#define AIC32X4_AOSR		20
#define AIC32X4_CLKMUX2		25
#define AIC32X4_CLKOUTM		26
#define AIC32X4_IFACE1		27
#define AIC32X4_IFACE2		28
#define AIC32X4_IFACE3		29
#define AIC32X4_BCLKN		30
#define AIC32X4_IFACE4		31
#define AIC32X4_IFACE5		32
#define AIC32X4_IFACE6		33
#define AIC32X4_DOUTCTL		53
#define AIC32X4_DINCTL		54
#define AIC32X4_DACSPB		60
#define AIC32X4_ADCSPB		61
#define AIC32X4_DACSETUP	63
#define AIC32X4_DACMUTE		64
#define AIC32X4_LDACVOL		65
#define AIC32X4_RDACVOL		66
#define AIC32X4_ADCSETUP	81
#define	AIC32X4_ADCFGA		82
#define AIC32X4_LADCVOL		83
#define AIC32X4_RADCVOL		84
#define AIC32X4_LAGC1		86
#define AIC32X4_LAGC2		87
#define AIC32X4_LAGC3		88
#define AIC32X4_LAGC4		89
#define AIC32X4_LAGC5		90
#define AIC32X4_LAGC6		91
#define AIC32X4_LAGC7		92
#define AIC32X4_RAGC1		94
#define AIC32X4_RAGC2		95
#define AIC32X4_RAGC3		96
#define AIC32X4_RAGC4		97
#define AIC32X4_RAGC5		98
#define AIC32X4_RAGC6		99
#define AIC32X4_RAGC7		100
#define AIC32X4_PWRCFG		(AIC32X4_PAGE1 + 1)
#define AIC32X4_LDOCTL		(AIC32X4_PAGE1 + 2)
#define AIC32X4_OUTPWRCTL	(AIC32X4_PAGE1 + 9)
#define AIC32X4_CMMODE		(AIC32X4_PAGE1 + 10)
#define AIC32X4_HPLROUTE	(AIC32X4_PAGE1 + 12)
#define AIC32X4_HPRROUTE	(AIC32X4_PAGE1 + 13)
#define AIC32X4_LOLROUTE	(AIC32X4_PAGE1 + 14)
#define AIC32X4_LORROUTE	(AIC32X4_PAGE1 + 15)
#define	AIC32X4_HPLGAIN		(AIC32X4_PAGE1 + 16)
#define	AIC32X4_HPRGAIN		(AIC32X4_PAGE1 + 17)
#define	AIC32X4_LOLGAIN		(AIC32X4_PAGE1 + 18)
#define	AIC32X4_LORGAIN		(AIC32X4_PAGE1 + 19)
#define AIC32X4_HEADSTART	(AIC32X4_PAGE1 + 20)
#define AIC32X4_MICBIAS		(AIC32X4_PAGE1 + 51)
#define AIC32X4_LMICPGAPIN	(AIC32X4_PAGE1 + 52)
#define AIC32X4_LMICPGANIN	(AIC32X4_PAGE1 + 54)
#define AIC32X4_RMICPGAPIN	(AIC32X4_PAGE1 + 55)
#define AIC32X4_RMICPGANIN	(AIC32X4_PAGE1 + 57)
#define AIC32X4_FLOATINGINPUT	(AIC32X4_PAGE1 + 58)
#define AIC32X4_LMICPGAVOL	(AIC32X4_PAGE1 + 59)
#define AIC32X4_RMICPGAVOL	(AIC32X4_PAGE1 + 60)

#define AIC32X4_FREQ_12000000 12000000
#define AIC32X4_FREQ_24000000 24000000
#define AIC32X4_FREQ_25000000 25000000

#define AIC32X4_WORD_LEN_16BITS		0x00
#define AIC32X4_WORD_LEN_20BITS		0x01
#define AIC32X4_WORD_LEN_24BITS		0x02
#define AIC32X4_WORD_LEN_32BITS		0x03

#define AIC32X4_I2S_MODE		0x00
#define AIC32X4_DSP_MODE		0x01
#define AIC32X4_RIGHT_JUSTIFIED_MODE	0x02
#define AIC32X4_LEFT_JUSTIFIED_MODE	0x03

#define AIC32X4_AVDDWEAKDISABLE		0x08
#define AIC32X4_LDOCTLEN		0x01

#define AIC32X4_LDOIN_18_36		0x01
#define AIC32X4_LDOIN2HP		0x02

#define AIC32X4_DACSPBLOCK_MASK		0x1f
#define AIC32X4_ADCSPBLOCK_MASK		0x1f

#define AIC32X4_PLLJ_SHIFT		6
#define AIC32X4_DOSRMSB_SHIFT		4

#define AIC32X4_PLLCLKIN		0x03

#define AIC32X4_MICBIAS_LDOIN		0x08
#define AIC32X4_MICBIAS_2075V		0x60

#define AIC32X4_LMICPGANIN_IN2R_10K	0x10
#define AIC32X4_RMICPGANIN_IN1L_10K	0x10

#define AIC32X4_LMICPGAVOL_NOGAIN	0x80
#define AIC32X4_RMICPGAVOL_NOGAIN	0x80

#define AIC32X4_BCLKMASTER		0x08
#define AIC32X4_WCLKMASTER		0x04
#define AIC32X4_PLLEN			(0x01 << 7)
#define AIC32X4_NDACEN			(0x01 << 7)
#define AIC32X4_MDACEN			(0x01 << 7)
#define AIC32X4_NADCEN			(0x01 << 7)
#define AIC32X4_MADCEN			(0x01 << 7)
#define AIC32X4_BCLKEN			(0x01 << 7)
#define AIC32X4_DACEN			(0x03 << 6)
#define AIC32X4_RDAC2LCHN		(0x02 << 2)
#define AIC32X4_LDAC2RCHN		(0x02 << 4)
#define AIC32X4_LDAC2LCHN		(0x01 << 4)
#define AIC32X4_RDAC2RCHN		(0x01 << 2)

#define AIC32X4_SSTEP2WCLK		0x01
#define AIC32X4_MUTEON			0x0C
#define	AIC32X4_DACMOD2BCLK		0x01

#endif				/* _TLV320AIC32X4_H */
