/*

  Broadcom B43 wireless driver

  Copyright (c) 2010 Rafał Miłecki <zajec5@gmail.com>

  Some parts of the code in this file are derived from the brcm80211
  driver  Copyright (c) 2010 Broadcom Corporation

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#ifndef B43_RADIO_2056_H_
#define B43_RADIO_2056_H_

#include <linux/types.h>

#include "tables_nphy.h"

#define B2056_SYN			(0x0 << 12)
#define B2056_TX0			(0x2 << 12)
#define B2056_TX1			(0x3 << 12)
#define B2056_RX0			(0x6 << 12)
#define B2056_RX1			(0x7 << 12)
#define B2056_ALLTX			(0xE << 12)
#define B2056_ALLRX			(0xF << 12)

#define B2056_SYN_RESERVED_ADDR0	0x00
#define B2056_SYN_IDCODE		0x01
#define B2056_SYN_RESERVED_ADDR2	0x02
#define B2056_SYN_RESERVED_ADDR3	0x03
#define B2056_SYN_RESERVED_ADDR4	0x04
#define B2056_SYN_RESERVED_ADDR5	0x05
#define B2056_SYN_RESERVED_ADDR6	0x06
#define B2056_SYN_RESERVED_ADDR7	0x07
#define B2056_SYN_COM_CTRL		0x08
#define B2056_SYN_COM_PU		0x09
#define B2056_SYN_COM_OVR		0x0A
#define B2056_SYN_COM_RESET		0x0B
#define B2056_SYN_COM_RCAL		0x0C
#define B2056_SYN_COM_RC_RXLPF		0x0D
#define B2056_SYN_COM_RC_TXLPF		0x0E
#define B2056_SYN_COM_RC_RXHPF		0x0F
#define B2056_SYN_RESERVED_ADDR16	0x10
#define B2056_SYN_RESERVED_ADDR17	0x11
#define B2056_SYN_RESERVED_ADDR18	0x12
#define B2056_SYN_RESERVED_ADDR19	0x13
#define B2056_SYN_RESERVED_ADDR20	0x14
#define B2056_SYN_RESERVED_ADDR21	0x15
#define B2056_SYN_RESERVED_ADDR22	0x16
#define B2056_SYN_RESERVED_ADDR23	0x17
#define B2056_SYN_RESERVED_ADDR24	0x18
#define B2056_SYN_RESERVED_ADDR25	0x19
#define B2056_SYN_RESERVED_ADDR26	0x1A
#define B2056_SYN_RESERVED_ADDR27	0x1B
#define B2056_SYN_RESERVED_ADDR28	0x1C
#define B2056_SYN_RESERVED_ADDR29	0x1D
#define B2056_SYN_RESERVED_ADDR30	0x1E
#define B2056_SYN_RESERVED_ADDR31	0x1F
#define B2056_SYN_GPIO_MASTER1		0x20
#define B2056_SYN_GPIO_MASTER2		0x21
#define B2056_SYN_TOPBIAS_MASTER	0x22
#define B2056_SYN_TOPBIAS_RCAL		0x23
#define B2056_SYN_AFEREG		0x24
#define B2056_SYN_TEMPPROCSENSE		0x25
#define B2056_SYN_TEMPPROCSENSEIDAC	0x26
#define B2056_SYN_TEMPPROCSENSERCAL	0x27
#define B2056_SYN_LPO			0x28
#define B2056_SYN_VDDCAL_MASTER		0x29
#define B2056_SYN_VDDCAL_IDAC		0x2A
#define B2056_SYN_VDDCAL_STATUS		0x2B
#define B2056_SYN_RCAL_MASTER		0x2C
#define B2056_SYN_RCAL_CODE_OUT		0x2D
#define B2056_SYN_RCCAL_CTRL0		0x2E
#define B2056_SYN_RCCAL_CTRL1		0x2F
#define B2056_SYN_RCCAL_CTRL2		0x30
#define B2056_SYN_RCCAL_CTRL3		0x31
#define B2056_SYN_RCCAL_CTRL4		0x32
#define B2056_SYN_RCCAL_CTRL5		0x33
#define B2056_SYN_RCCAL_CTRL6		0x34
#define B2056_SYN_RCCAL_CTRL7		0x35
#define B2056_SYN_RCCAL_CTRL8		0x36
#define B2056_SYN_RCCAL_CTRL9		0x37
#define B2056_SYN_RCCAL_CTRL10		0x38
#define B2056_SYN_RCCAL_CTRL11		0x39
#define B2056_SYN_ZCAL_SPARE1		0x3A
#define B2056_SYN_ZCAL_SPARE2		0x3B
#define B2056_SYN_PLL_MAST1		0x3C
#define B2056_SYN_PLL_MAST2		0x3D
#define B2056_SYN_PLL_MAST3		0x3E
#define B2056_SYN_PLL_BIAS_RESET	0x3F
#define B2056_SYN_PLL_XTAL0		0x40
#define B2056_SYN_PLL_XTAL1		0x41
#define B2056_SYN_PLL_XTAL3		0x42
#define B2056_SYN_PLL_XTAL4		0x43
#define B2056_SYN_PLL_XTAL5		0x44
#define B2056_SYN_PLL_XTAL6		0x45
#define B2056_SYN_PLL_REFDIV		0x46
#define B2056_SYN_PLL_PFD		0x47
#define B2056_SYN_PLL_CP1		0x48
#define B2056_SYN_PLL_CP2		0x49
#define B2056_SYN_PLL_CP3		0x4A
#define B2056_SYN_PLL_LOOPFILTER1	0x4B
#define B2056_SYN_PLL_LOOPFILTER2	0x4C
#define B2056_SYN_PLL_LOOPFILTER3	0x4D
#define B2056_SYN_PLL_LOOPFILTER4	0x4E
#define B2056_SYN_PLL_LOOPFILTER5	0x4F
#define B2056_SYN_PLL_MMD1		0x50
#define B2056_SYN_PLL_MMD2		0x51
#define B2056_SYN_PLL_VCO1		0x52
#define B2056_SYN_PLL_VCO2		0x53
#define B2056_SYN_PLL_MONITOR1		0x54
#define B2056_SYN_PLL_MONITOR2		0x55
#define B2056_SYN_PLL_VCOCAL1		0x56
#define B2056_SYN_PLL_VCOCAL2		0x57
#define B2056_SYN_PLL_VCOCAL4		0x58
#define B2056_SYN_PLL_VCOCAL5		0x59
#define B2056_SYN_PLL_VCOCAL6		0x5A
#define B2056_SYN_PLL_VCOCAL7		0x5B
#define B2056_SYN_PLL_VCOCAL8		0x5C
#define B2056_SYN_PLL_VCOCAL9		0x5D
#define B2056_SYN_PLL_VCOCAL10		0x5E
#define B2056_SYN_PLL_VCOCAL11		0x5F
#define B2056_SYN_PLL_VCOCAL12		0x60
#define B2056_SYN_PLL_VCOCAL13		0x61
#define B2056_SYN_PLL_VREG		0x62
#define B2056_SYN_PLL_STATUS1		0x63
#define B2056_SYN_PLL_STATUS2		0x64
#define B2056_SYN_PLL_STATUS3		0x65
#define B2056_SYN_LOGEN_PU0		0x66
#define B2056_SYN_LOGEN_PU1		0x67
#define B2056_SYN_LOGEN_PU2		0x68
#define B2056_SYN_LOGEN_PU3		0x69
#define B2056_SYN_LOGEN_PU5		0x6A
#define B2056_SYN_LOGEN_PU6		0x6B
#define B2056_SYN_LOGEN_PU7		0x6C
#define B2056_SYN_LOGEN_PU8		0x6D
#define B2056_SYN_LOGEN_BIAS_RESET	0x6E
#define B2056_SYN_LOGEN_RCCR1		0x6F
#define B2056_SYN_LOGEN_VCOBUF1		0x70
#define B2056_SYN_LOGEN_MIXER1		0x71
#define B2056_SYN_LOGEN_MIXER2		0x72
#define B2056_SYN_LOGEN_BUF1		0x73
#define B2056_SYN_LOGENBUF2		0x74
#define B2056_SYN_LOGEN_BUF3		0x75
#define B2056_SYN_LOGEN_BUF4		0x76
#define B2056_SYN_LOGEN_DIV1		0x77
#define B2056_SYN_LOGEN_DIV2		0x78
#define B2056_SYN_LOGEN_DIV3		0x79
#define B2056_SYN_LOGEN_ACL1		0x7A
#define B2056_SYN_LOGEN_ACL2		0x7B
#define B2056_SYN_LOGEN_ACL3		0x7C
#define B2056_SYN_LOGEN_ACL4		0x7D
#define B2056_SYN_LOGEN_ACL5		0x7E
#define B2056_SYN_LOGEN_ACL6		0x7F
#define B2056_SYN_LOGEN_ACLOUT		0x80
#define B2056_SYN_LOGEN_ACLCAL1		0x81
#define B2056_SYN_LOGEN_ACLCAL2		0x82
#define B2056_SYN_LOGEN_ACLCAL3		0x83
#define B2056_SYN_CALEN			0x84
#define B2056_SYN_LOGEN_PEAKDET1	0x85
#define B2056_SYN_LOGEN_CORE_ACL_OVR	0x86
#define B2056_SYN_LOGEN_RX_DIFF_ACL_OVR	0x87
#define B2056_SYN_LOGEN_TX_DIFF_ACL_OVR	0x88
#define B2056_SYN_LOGEN_RX_CMOS_ACL_OVR	0x89
#define B2056_SYN_LOGEN_TX_CMOS_ACL_OVR	0x8A
#define B2056_SYN_LOGEN_VCOBUF2		0x8B
#define B2056_SYN_LOGEN_MIXER3		0x8C
#define B2056_SYN_LOGEN_BUF5		0x8D
#define B2056_SYN_LOGEN_BUF6		0x8E
#define B2056_SYN_LOGEN_CBUFRX1		0x8F
#define B2056_SYN_LOGEN_CBUFRX2		0x90
#define B2056_SYN_LOGEN_CBUFRX3		0x91
#define B2056_SYN_LOGEN_CBUFRX4		0x92
#define B2056_SYN_LOGEN_CBUFTX1		0x93
#define B2056_SYN_LOGEN_CBUFTX2		0x94
#define B2056_SYN_LOGEN_CBUFTX3		0x95
#define B2056_SYN_LOGEN_CBUFTX4		0x96
#define B2056_SYN_LOGEN_CMOSRX1		0x97
#define B2056_SYN_LOGEN_CMOSRX2		0x98
#define B2056_SYN_LOGEN_CMOSRX3		0x99
#define B2056_SYN_LOGEN_CMOSRX4		0x9A
#define B2056_SYN_LOGEN_CMOSTX1		0x9B
#define B2056_SYN_LOGEN_CMOSTX2		0x9C
#define B2056_SYN_LOGEN_CMOSTX3		0x9D
#define B2056_SYN_LOGEN_CMOSTX4		0x9E
#define B2056_SYN_LOGEN_VCOBUF2_OVRVAL	0x9F
#define B2056_SYN_LOGEN_MIXER3_OVRVAL	0xA0
#define B2056_SYN_LOGEN_BUF5_OVRVAL	0xA1
#define B2056_SYN_LOGEN_BUF6_OVRVAL	0xA2
#define B2056_SYN_LOGEN_CBUFRX1_OVRVAL	0xA3
#define B2056_SYN_LOGEN_CBUFRX2_OVRVAL	0xA4
#define B2056_SYN_LOGEN_CBUFRX3_OVRVAL	0xA5
#define B2056_SYN_LOGEN_CBUFRX4_OVRVAL	0xA6
#define B2056_SYN_LOGEN_CBUFTX1_OVRVAL	0xA7
#define B2056_SYN_LOGEN_CBUFTX2_OVRVAL	0xA8
#define B2056_SYN_LOGEN_CBUFTX3_OVRVAL	0xA9
#define B2056_SYN_LOGEN_CBUFTX4_OVRVAL	0xAA
#define B2056_SYN_LOGEN_CMOSRX1_OVRVAL	0xAB
#define B2056_SYN_LOGEN_CMOSRX2_OVRVAL	0xAC
#define B2056_SYN_LOGEN_CMOSRX3_OVRVAL	0xAD
#define B2056_SYN_LOGEN_CMOSRX4_OVRVAL	0xAE
#define B2056_SYN_LOGEN_CMOSTX1_OVRVAL	0xAF
#define B2056_SYN_LOGEN_CMOSTX2_OVRVAL	0xB0
#define B2056_SYN_LOGEN_CMOSTX3_OVRVAL	0xB1
#define B2056_SYN_LOGEN_CMOSTX4_OVRVAL	0xB2
#define B2056_SYN_LOGEN_ACL_WAITCNT	0xB3
#define B2056_SYN_LOGEN_CORE_CALVALID	0xB4
#define B2056_SYN_LOGEN_RX_CMOS_CALVALID	0xB5
#define B2056_SYN_LOGEN_TX_CMOS_VALID	0xB6

#define B2056_TX_RESERVED_ADDR0		0x00
#define B2056_TX_IDCODE			0x01
#define B2056_TX_RESERVED_ADDR2		0x02
#define B2056_TX_RESERVED_ADDR3		0x03
#define B2056_TX_RESERVED_ADDR4		0x04
#define B2056_TX_RESERVED_ADDR5		0x05
#define B2056_TX_RESERVED_ADDR6		0x06
#define B2056_TX_RESERVED_ADDR7		0x07
#define B2056_TX_COM_CTRL		0x08
#define B2056_TX_COM_PU			0x09
#define B2056_TX_COM_OVR		0x0A
#define B2056_TX_COM_RESET		0x0B
#define B2056_TX_COM_RCAL		0x0C
#define B2056_TX_COM_RC_RXLPF		0x0D
#define B2056_TX_COM_RC_TXLPF		0x0E
#define B2056_TX_COM_RC_RXHPF		0x0F
#define B2056_TX_RESERVED_ADDR16	0x10
#define B2056_TX_RESERVED_ADDR17	0x11
#define B2056_TX_RESERVED_ADDR18	0x12
#define B2056_TX_RESERVED_ADDR19	0x13
#define B2056_TX_RESERVED_ADDR20	0x14
#define B2056_TX_RESERVED_ADDR21	0x15
#define B2056_TX_RESERVED_ADDR22	0x16
#define B2056_TX_RESERVED_ADDR23	0x17
#define B2056_TX_RESERVED_ADDR24	0x18
#define B2056_TX_RESERVED_ADDR25	0x19
#define B2056_TX_RESERVED_ADDR26	0x1A
#define B2056_TX_RESERVED_ADDR27	0x1B
#define B2056_TX_RESERVED_ADDR28	0x1C
#define B2056_TX_RESERVED_ADDR29	0x1D
#define B2056_TX_RESERVED_ADDR30	0x1E
#define B2056_TX_RESERVED_ADDR31	0x1F
#define B2056_TX_IQCAL_GAIN_BW		0x20
#define B2056_TX_LOFT_FINE_I		0x21
#define B2056_TX_LOFT_FINE_Q		0x22
#define B2056_TX_LOFT_COARSE_I		0x23
#define B2056_TX_LOFT_COARSE_Q		0x24
#define B2056_TX_TX_COM_MASTER1		0x25
#define B2056_TX_TX_COM_MASTER2		0x26
#define B2056_TX_RXIQCAL_TXMUX		0x27
#define B2056_TX_TX_SSI_MASTER		0x28
#define B2056_TX_IQCAL_VCM_HG		0x29
#define B2056_TX_IQCAL_IDAC		0x2A
#define B2056_TX_TSSI_VCM		0x2B
#define B2056_TX_TX_AMP_DET		0x2C
#define B2056_TX_TX_SSI_MUX		0x2D
#define B2056_TX_TSSIA			0x2E
#define B2056_TX_TSSIG			0x2F
#define B2056_TX_TSSI_MISC1		0x30
#define B2056_TX_TSSI_MISC2		0x31
#define B2056_TX_TSSI_MISC3		0x32
#define B2056_TX_PA_SPARE1		0x33
#define B2056_TX_PA_SPARE2		0x34
#define B2056_TX_INTPAA_MASTER		0x35
#define B2056_TX_INTPAA_GAIN		0x36
#define B2056_TX_INTPAA_BOOST_TUNE	0x37
#define B2056_TX_INTPAA_IAUX_STAT	0x38
#define B2056_TX_INTPAA_IAUX_DYN	0x39
#define B2056_TX_INTPAA_IMAIN_STAT	0x3A
#define B2056_TX_INTPAA_IMAIN_DYN	0x3B
#define B2056_TX_INTPAA_CASCBIAS	0x3C
#define B2056_TX_INTPAA_PASLOPE		0x3D
#define B2056_TX_INTPAA_PA_MISC		0x3E
#define B2056_TX_INTPAG_MASTER		0x3F
#define B2056_TX_INTPAG_GAIN		0x40
#define B2056_TX_INTPAG_BOOST_TUNE	0x41
#define B2056_TX_INTPAG_IAUX_STAT	0x42
#define B2056_TX_INTPAG_IAUX_DYN	0x43
#define B2056_TX_INTPAG_IMAIN_STAT	0x44
#define B2056_TX_INTPAG_IMAIN_DYN	0x45
#define B2056_TX_INTPAG_CASCBIAS	0x46
#define B2056_TX_INTPAG_PASLOPE		0x47
#define B2056_TX_INTPAG_PA_MISC		0x48
#define B2056_TX_PADA_MASTER		0x49
#define B2056_TX_PADA_IDAC		0x4A
#define B2056_TX_PADA_CASCBIAS		0x4B
#define B2056_TX_PADA_GAIN		0x4C
#define B2056_TX_PADA_BOOST_TUNE	0x4D
#define B2056_TX_PADA_SLOPE		0x4E
#define B2056_TX_PADG_MASTER		0x4F
#define B2056_TX_PADG_IDAC		0x50
#define B2056_TX_PADG_CASCBIAS		0x51
#define B2056_TX_PADG_GAIN		0x52
#define B2056_TX_PADG_BOOST_TUNE	0x53
#define B2056_TX_PADG_SLOPE		0x54
#define B2056_TX_PGAA_MASTER		0x55
#define B2056_TX_PGAA_IDAC		0x56
#define B2056_TX_PGAA_GAIN		0x57
#define B2056_TX_PGAA_BOOST_TUNE	0x58
#define B2056_TX_PGAA_SLOPE		0x59
#define B2056_TX_PGAA_MISC		0x5A
#define B2056_TX_PGAG_MASTER		0x5B
#define B2056_TX_PGAG_IDAC		0x5C
#define B2056_TX_PGAG_GAIN		0x5D
#define B2056_TX_PGAG_BOOST_TUNE	0x5E
#define B2056_TX_PGAG_SLOPE		0x5F
#define B2056_TX_PGAG_MISC		0x60
#define B2056_TX_MIXA_MASTER		0x61
#define B2056_TX_MIXA_BOOST_TUNE	0x62
#define B2056_TX_MIXG			0x63
#define B2056_TX_MIXG_BOOST_TUNE	0x64
#define B2056_TX_BB_GM_MASTER		0x65
#define B2056_TX_GMBB_GM		0x66
#define B2056_TX_GMBB_IDAC		0x67
#define B2056_TX_TXLPF_MASTER		0x68
#define B2056_TX_TXLPF_RCCAL		0x69
#define B2056_TX_TXLPF_RCCAL_OFF0	0x6A
#define B2056_TX_TXLPF_RCCAL_OFF1	0x6B
#define B2056_TX_TXLPF_RCCAL_OFF2	0x6C
#define B2056_TX_TXLPF_RCCAL_OFF3	0x6D
#define B2056_TX_TXLPF_RCCAL_OFF4	0x6E
#define B2056_TX_TXLPF_RCCAL_OFF5	0x6F
#define B2056_TX_TXLPF_RCCAL_OFF6	0x70
#define B2056_TX_TXLPF_BW		0x71
#define B2056_TX_TXLPF_GAIN		0x72
#define B2056_TX_TXLPF_IDAC		0x73
#define B2056_TX_TXLPF_IDAC_0		0x74
#define B2056_TX_TXLPF_IDAC_1		0x75
#define B2056_TX_TXLPF_IDAC_2		0x76
#define B2056_TX_TXLPF_IDAC_3		0x77
#define B2056_TX_TXLPF_IDAC_4		0x78
#define B2056_TX_TXLPF_IDAC_5		0x79
#define B2056_TX_TXLPF_IDAC_6		0x7A
#define B2056_TX_TXLPF_OPAMP_IDAC	0x7B
#define B2056_TX_TXLPF_MISC		0x7C
#define B2056_TX_TXSPARE1		0x7D
#define B2056_TX_TXSPARE2		0x7E
#define B2056_TX_TXSPARE3		0x7F
#define B2056_TX_TXSPARE4		0x80
#define B2056_TX_TXSPARE5		0x81
#define B2056_TX_TXSPARE6		0x82
#define B2056_TX_TXSPARE7		0x83
#define B2056_TX_TXSPARE8		0x84
#define B2056_TX_TXSPARE9		0x85
#define B2056_TX_TXSPARE10		0x86
#define B2056_TX_TXSPARE11		0x87
#define B2056_TX_TXSPARE12		0x88
#define B2056_TX_TXSPARE13		0x89
#define B2056_TX_TXSPARE14		0x8A
#define B2056_TX_TXSPARE15		0x8B
#define B2056_TX_TXSPARE16		0x8C
#define B2056_TX_STATUS_INTPA_GAIN	0x8D
#define B2056_TX_STATUS_PAD_GAIN	0x8E
#define B2056_TX_STATUS_PGA_GAIN	0x8F
#define B2056_TX_STATUS_GM_TXLPF_GAIN	0x90
#define B2056_TX_STATUS_TXLPF_BW	0x91
#define B2056_TX_STATUS_TXLPF_RC	0x92
#define B2056_TX_GMBB_IDAC0		0x93
#define B2056_TX_GMBB_IDAC1		0x94
#define B2056_TX_GMBB_IDAC2		0x95
#define B2056_TX_GMBB_IDAC3		0x96
#define B2056_TX_GMBB_IDAC4		0x97
#define B2056_TX_GMBB_IDAC5		0x98
#define B2056_TX_GMBB_IDAC6		0x99
#define B2056_TX_GMBB_IDAC7		0x9A

#define B2056_RX_RESERVED_ADDR0		0x00
#define B2056_RX_IDCODE			0x01
#define B2056_RX_RESERVED_ADDR2		0x02
#define B2056_RX_RESERVED_ADDR3		0x03
#define B2056_RX_RESERVED_ADDR4		0x04
#define B2056_RX_RESERVED_ADDR5		0x05
#define B2056_RX_RESERVED_ADDR6		0x06
#define B2056_RX_RESERVED_ADDR7		0x07
#define B2056_RX_COM_CTRL		0x08
#define B2056_RX_COM_PU			0x09
#define B2056_RX_COM_OVR		0x0A
#define B2056_RX_COM_RESET		0x0B
#define B2056_RX_COM_RCAL		0x0C
#define B2056_RX_COM_RC_RXLPF		0x0D
#define B2056_RX_COM_RC_TXLPF		0x0E
#define B2056_RX_COM_RC_RXHPF		0x0F
#define B2056_RX_RESERVED_ADDR16	0x10
#define B2056_RX_RESERVED_ADDR17	0x11
#define B2056_RX_RESERVED_ADDR18	0x12
#define B2056_RX_RESERVED_ADDR19	0x13
#define B2056_RX_RESERVED_ADDR20	0x14
#define B2056_RX_RESERVED_ADDR21	0x15
#define B2056_RX_RESERVED_ADDR22	0x16
#define B2056_RX_RESERVED_ADDR23	0x17
#define B2056_RX_RESERVED_ADDR24	0x18
#define B2056_RX_RESERVED_ADDR25	0x19
#define B2056_RX_RESERVED_ADDR26	0x1A
#define B2056_RX_RESERVED_ADDR27	0x1B
#define B2056_RX_RESERVED_ADDR28	0x1C
#define B2056_RX_RESERVED_ADDR29	0x1D
#define B2056_RX_RESERVED_ADDR30	0x1E
#define B2056_RX_RESERVED_ADDR31	0x1F
#define B2056_RX_RXIQCAL_RXMUX		0x20
#define B2056_RX_RSSI_PU		0x21
#define B2056_RX_RSSI_SEL		0x22
#define B2056_RX_RSSI_GAIN		0x23
#define B2056_RX_RSSI_NB_IDAC		0x24
#define B2056_RX_RSSI_WB2I_IDAC_1	0x25
#define B2056_RX_RSSI_WB2I_IDAC_2	0x26
#define B2056_RX_RSSI_WB2Q_IDAC_1	0x27
#define B2056_RX_RSSI_WB2Q_IDAC_2	0x28
#define B2056_RX_RSSI_POLE		0x29
#define B2056_RX_RSSI_WB1_IDAC		0x2A
#define B2056_RX_RSSI_MISC		0x2B
#define B2056_RX_LNAA_MASTER		0x2C
#define B2056_RX_LNAA_TUNE		0x2D
#define B2056_RX_LNAA_GAIN		0x2E
#define B2056_RX_LNA_A_SLOPE		0x2F
#define B2056_RX_BIASPOLE_LNAA1_IDAC	0x30
#define B2056_RX_LNAA2_IDAC		0x31
#define B2056_RX_LNA1A_MISC		0x32
#define B2056_RX_LNAG_MASTER		0x33
#define B2056_RX_LNAG_TUNE		0x34
#define B2056_RX_LNAG_GAIN		0x35
#define B2056_RX_LNA_G_SLOPE		0x36
#define B2056_RX_BIASPOLE_LNAG1_IDAC	0x37
#define B2056_RX_LNAG2_IDAC		0x38
#define B2056_RX_LNA1G_MISC		0x39
#define B2056_RX_MIXA_MASTER		0x3A
#define B2056_RX_MIXA_VCM		0x3B
#define B2056_RX_MIXA_CTRLPTAT		0x3C
#define B2056_RX_MIXA_LOB_BIAS		0x3D
#define B2056_RX_MIXA_CORE_IDAC		0x3E
#define B2056_RX_MIXA_CMFB_IDAC		0x3F
#define B2056_RX_MIXA_BIAS_AUX		0x40
#define B2056_RX_MIXA_BIAS_MAIN		0x41
#define B2056_RX_MIXA_BIAS_MISC		0x42
#define B2056_RX_MIXA_MAST_BIAS		0x43
#define B2056_RX_MIXG_MASTER		0x44
#define B2056_RX_MIXG_VCM		0x45
#define B2056_RX_MIXG_CTRLPTAT		0x46
#define B2056_RX_MIXG_LOB_BIAS		0x47
#define B2056_RX_MIXG_CORE_IDAC		0x48
#define B2056_RX_MIXG_CMFB_IDAC		0x49
#define B2056_RX_MIXG_BIAS_AUX		0x4A
#define B2056_RX_MIXG_BIAS_MAIN		0x4B
#define B2056_RX_MIXG_BIAS_MISC		0x4C
#define B2056_RX_MIXG_MAST_BIAS		0x4D
#define B2056_RX_TIA_MASTER		0x4E
#define B2056_RX_TIA_IOPAMP		0x4F
#define B2056_RX_TIA_QOPAMP		0x50
#define B2056_RX_TIA_IMISC		0x51
#define B2056_RX_TIA_QMISC		0x52
#define B2056_RX_TIA_GAIN		0x53
#define B2056_RX_TIA_SPARE1		0x54
#define B2056_RX_TIA_SPARE2		0x55
#define B2056_RX_BB_LPF_MASTER		0x56
#define B2056_RX_AACI_MASTER		0x57
#define B2056_RX_RXLPF_IDAC		0x58
#define B2056_RX_RXLPF_OPAMPBIAS_LOWQ	0x59
#define B2056_RX_RXLPF_OPAMPBIAS_HIGHQ	0x5A
#define B2056_RX_RXLPF_BIAS_DCCANCEL	0x5B
#define B2056_RX_RXLPF_OUTVCM		0x5C
#define B2056_RX_RXLPF_INVCM_BODY	0x5D
#define B2056_RX_RXLPF_CC_OP		0x5E
#define B2056_RX_RXLPF_GAIN		0x5F
#define B2056_RX_RXLPF_Q_BW		0x60
#define B2056_RX_RXLPF_HP_CORNER_BW	0x61
#define B2056_RX_RXLPF_RCCAL_HPC	0x62
#define B2056_RX_RXHPF_OFF0		0x63
#define B2056_RX_RXHPF_OFF1		0x64
#define B2056_RX_RXHPF_OFF2		0x65
#define B2056_RX_RXHPF_OFF3		0x66
#define B2056_RX_RXHPF_OFF4		0x67
#define B2056_RX_RXHPF_OFF5		0x68
#define B2056_RX_RXHPF_OFF6		0x69
#define B2056_RX_RXHPF_OFF7		0x6A
#define B2056_RX_RXLPF_RCCAL_LPC	0x6B
#define B2056_RX_RXLPF_OFF_0		0x6C
#define B2056_RX_RXLPF_OFF_1		0x6D
#define B2056_RX_RXLPF_OFF_2		0x6E
#define B2056_RX_RXLPF_OFF_3		0x6F
#define B2056_RX_RXLPF_OFF_4		0x70
#define B2056_RX_UNUSED			0x71
#define B2056_RX_VGA_MASTER		0x72
#define B2056_RX_VGA_BIAS		0x73
#define B2056_RX_VGA_BIAS_DCCANCEL	0x74
#define B2056_RX_VGA_GAIN		0x75
#define B2056_RX_VGA_HP_CORNER_BW	0x76
#define B2056_RX_VGABUF_BIAS		0x77
#define B2056_RX_VGABUF_GAIN_BW		0x78
#define B2056_RX_TXFBMIX_A		0x79
#define B2056_RX_TXFBMIX_G		0x7A
#define B2056_RX_RXSPARE1		0x7B
#define B2056_RX_RXSPARE2		0x7C
#define B2056_RX_RXSPARE3		0x7D
#define B2056_RX_RXSPARE4		0x7E
#define B2056_RX_RXSPARE5		0x7F
#define B2056_RX_RXSPARE6		0x80
#define B2056_RX_RXSPARE7		0x81
#define B2056_RX_RXSPARE8		0x82
#define B2056_RX_RXSPARE9		0x83
#define B2056_RX_RXSPARE10		0x84
#define B2056_RX_RXSPARE11		0x85
#define B2056_RX_RXSPARE12		0x86
#define B2056_RX_RXSPARE13		0x87
#define B2056_RX_RXSPARE14		0x88
#define B2056_RX_RXSPARE15		0x89
#define B2056_RX_RXSPARE16		0x8A
#define B2056_RX_STATUS_LNAA_GAIN	0x8B
#define B2056_RX_STATUS_LNAG_GAIN	0x8C
#define B2056_RX_STATUS_MIXTIA_GAIN	0x8D
#define B2056_RX_STATUS_RXLPF_GAIN	0x8E
#define B2056_RX_STATUS_VGA_BUF_GAIN	0x8F
#define B2056_RX_STATUS_RXLPF_Q		0x90
#define B2056_RX_STATUS_RXLPF_BUF_BW	0x91
#define B2056_RX_STATUS_RXLPF_VGA_HPC	0x92
#define B2056_RX_STATUS_RXLPF_RC	0x93
#define B2056_RX_STATUS_HPC_RC		0x94

#define B2056_LNA1_A_PU			0x01
#define B2056_LNA2_A_PU			0x02
#define B2056_LNA1_G_PU			0x01
#define B2056_LNA2_G_PU			0x02
#define B2056_MIXA_PU_I			0x01
#define B2056_MIXA_PU_Q			0x02
#define B2056_MIXA_PU_GM		0x10
#define B2056_MIXG_PU_I			0x01
#define B2056_MIXG_PU_Q			0x02
#define B2056_MIXG_PU_GM		0x10
#define B2056_TIA_PU			0x01
#define B2056_BB_LPF_PU			0x20
#define B2056_W1_PU			0x02
#define B2056_W2_PU			0x04
#define B2056_NB_PU			0x08
#define B2056_RSSI_W1_SEL		0x02
#define B2056_RSSI_W2_SEL		0x04
#define B2056_RSSI_NB_SEL		0x08
#define B2056_VCM_MASK			0x1C
#define B2056_RSSI_VCM_SHIFT		0x02

#define B2056_SYN			(0x0 << 12)
#define B2056_TX0			(0x2 << 12)
#define B2056_TX1			(0x3 << 12)
#define B2056_RX0			(0x6 << 12)
#define B2056_RX1			(0x7 << 12)
#define B2056_ALLTX			(0xE << 12)
#define B2056_ALLRX			(0xF << 12)

#define B2056_SYN_RESERVED_ADDR0	0x00
#define B2056_SYN_IDCODE		0x01
#define B2056_SYN_RESERVED_ADDR2	0x02
#define B2056_SYN_RESERVED_ADDR3	0x03
#define B2056_SYN_RESERVED_ADDR4	0x04
#define B2056_SYN_RESERVED_ADDR5	0x05
#define B2056_SYN_RESERVED_ADDR6	0x06
#define B2056_SYN_RESERVED_ADDR7	0x07
#define B2056_SYN_COM_CTRL		0x08
#define B2056_SYN_COM_PU		0x09
#define B2056_SYN_COM_OVR		0x0A
#define B2056_SYN_COM_RESET		0x0B
#define B2056_SYN_COM_RCAL		0x0C
#define B2056_SYN_COM_RC_RXLPF		0x0D
#define B2056_SYN_COM_RC_TXLPF		0x0E
#define B2056_SYN_COM_RC_RXHPF		0x0F
#define B2056_SYN_RESERVED_ADDR16	0x10
#define B2056_SYN_RESERVED_ADDR17	0x11
#define B2056_SYN_RESERVED_ADDR18	0x12
#define B2056_SYN_RESERVED_ADDR19	0x13
#define B2056_SYN_RESERVED_ADDR20	0x14
#define B2056_SYN_RESERVED_ADDR21	0x15
#define B2056_SYN_RESERVED_ADDR22	0x16
#define B2056_SYN_RESERVED_ADDR23	0x17
#define B2056_SYN_RESERVED_ADDR24	0x18
#define B2056_SYN_RESERVED_ADDR25	0x19
#define B2056_SYN_RESERVED_ADDR26	0x1A
#define B2056_SYN_RESERVED_ADDR27	0x1B
#define B2056_SYN_RESERVED_ADDR28	0x1C
#define B2056_SYN_RESERVED_ADDR29	0x1D
#define B2056_SYN_RESERVED_ADDR30	0x1E
#define B2056_SYN_RESERVED_ADDR31	0x1F
#define B2056_SYN_GPIO_MASTER1		0x20
#define B2056_SYN_GPIO_MASTER2		0x21
#define B2056_SYN_TOPBIAS_MASTER	0x22
#define B2056_SYN_TOPBIAS_RCAL		0x23
#define B2056_SYN_AFEREG		0x24
#define B2056_SYN_TEMPPROCSENSE		0x25
#define B2056_SYN_TEMPPROCSENSEIDAC	0x26
#define B2056_SYN_TEMPPROCSENSERCAL	0x27
#define B2056_SYN_LPO			0x28
#define B2056_SYN_VDDCAL_MASTER		0x29
#define B2056_SYN_VDDCAL_IDAC		0x2A
#define B2056_SYN_VDDCAL_STATUS		0x2B
#define B2056_SYN_RCAL_MASTER		0x2C
#define B2056_SYN_RCAL_CODE_OUT		0x2D
#define B2056_SYN_RCCAL_CTRL0		0x2E
#define B2056_SYN_RCCAL_CTRL1		0x2F
#define B2056_SYN_RCCAL_CTRL2		0x30
#define B2056_SYN_RCCAL_CTRL3		0x31
#define B2056_SYN_RCCAL_CTRL4		0x32
#define B2056_SYN_RCCAL_CTRL5		0x33
#define B2056_SYN_RCCAL_CTRL6		0x34
#define B2056_SYN_RCCAL_CTRL7		0x35
#define B2056_SYN_RCCAL_CTRL8		0x36
#define B2056_SYN_RCCAL_CTRL9		0x37
#define B2056_SYN_RCCAL_CTRL10		0x38
#define B2056_SYN_RCCAL_CTRL11		0x39
#define B2056_SYN_ZCAL_SPARE1		0x3A
#define B2056_SYN_ZCAL_SPARE2		0x3B
#define B2056_SYN_PLL_MAST1		0x3C
#define B2056_SYN_PLL_MAST2		0x3D
#define B2056_SYN_PLL_MAST3		0x3E
#define B2056_SYN_PLL_BIAS_RESET	0x3F
#define B2056_SYN_PLL_XTAL0		0x40
#define B2056_SYN_PLL_XTAL1		0x41
#define B2056_SYN_PLL_XTAL3		0x42
#define B2056_SYN_PLL_XTAL4		0x43
#define B2056_SYN_PLL_XTAL5		0x44
#define B2056_SYN_PLL_XTAL6		0x45
#define B2056_SYN_PLL_REFDIV		0x46
#define B2056_SYN_PLL_PFD		0x47
#define B2056_SYN_PLL_CP1		0x48
#define B2056_SYN_PLL_CP2		0x49
#define B2056_SYN_PLL_CP3		0x4A
#define B2056_SYN_PLL_LOOPFILTER1	0x4B
#define B2056_SYN_PLL_LOOPFILTER2	0x4C
#define B2056_SYN_PLL_LOOPFILTER3	0x4D
#define B2056_SYN_PLL_LOOPFILTER4	0x4E
#define B2056_SYN_PLL_LOOPFILTER5	0x4F
#define B2056_SYN_PLL_MMD1		0x50
#define B2056_SYN_PLL_MMD2		0x51
#define B2056_SYN_PLL_VCO1		0x52
#define B2056_SYN_PLL_VCO2		0x53
#define B2056_SYN_PLL_MONITOR1		0x54
#define B2056_SYN_PLL_MONITOR2		0x55
#define B2056_SYN_PLL_VCOCAL1		0x56
#define B2056_SYN_PLL_VCOCAL2		0x57
#define B2056_SYN_PLL_VCOCAL4		0x58
#define B2056_SYN_PLL_VCOCAL5		0x59
#define B2056_SYN_PLL_VCOCAL6		0x5A
#define B2056_SYN_PLL_VCOCAL7		0x5B
#define B2056_SYN_PLL_VCOCAL8		0x5C
#define B2056_SYN_PLL_VCOCAL9		0x5D
#define B2056_SYN_PLL_VCOCAL10		0x5E
#define B2056_SYN_PLL_VCOCAL11		0x5F
#define B2056_SYN_PLL_VCOCAL12		0x60
#define B2056_SYN_PLL_VCOCAL13		0x61
#define B2056_SYN_PLL_VREG		0x62
#define B2056_SYN_PLL_STATUS1		0x63
#define B2056_SYN_PLL_STATUS2		0x64
#define B2056_SYN_PLL_STATUS3		0x65
#define B2056_SYN_LOGEN_PU0		0x66
#define B2056_SYN_LOGEN_PU1		0x67
#define B2056_SYN_LOGEN_PU2		0x68
#define B2056_SYN_LOGEN_PU3		0x69
#define B2056_SYN_LOGEN_PU5		0x6A
#define B2056_SYN_LOGEN_PU6		0x6B
#define B2056_SYN_LOGEN_PU7		0x6C
#define B2056_SYN_LOGEN_PU8		0x6D
#define B2056_SYN_LOGEN_BIAS_RESET	0x6E
#define B2056_SYN_LOGEN_RCCR1		0x6F
#define B2056_SYN_LOGEN_VCOBUF1		0x70
#define B2056_SYN_LOGEN_MIXER1		0x71
#define B2056_SYN_LOGEN_MIXER2		0x72
#define B2056_SYN_LOGEN_BUF1		0x73
#define B2056_SYN_LOGENBUF2		0x74
#define B2056_SYN_LOGEN_BUF3		0x75
#define B2056_SYN_LOGEN_BUF4		0x76
#define B2056_SYN_LOGEN_DIV1		0x77
#define B2056_SYN_LOGEN_DIV2		0x78
#define B2056_SYN_LOGEN_DIV3		0x79
#define B2056_SYN_LOGEN_ACL1		0x7A
#define B2056_SYN_LOGEN_ACL2		0x7B
#define B2056_SYN_LOGEN_ACL3		0x7C
#define B2056_SYN_LOGEN_ACL4		0x7D
#define B2056_SYN_LOGEN_ACL5		0x7E
#define B2056_SYN_LOGEN_ACL6		0x7F
#define B2056_SYN_LOGEN_ACLOUT		0x80
#define B2056_SYN_LOGEN_ACLCAL1		0x81
#define B2056_SYN_LOGEN_ACLCAL2		0x82
#define B2056_SYN_LOGEN_ACLCAL3		0x83
#define B2056_SYN_CALEN			0x84
#define B2056_SYN_LOGEN_PEAKDET1	0x85
#define B2056_SYN_LOGEN_CORE_ACL_OVR	0x86
#define B2056_SYN_LOGEN_RX_DIFF_ACL_OVR	0x87
#define B2056_SYN_LOGEN_TX_DIFF_ACL_OVR	0x88
#define B2056_SYN_LOGEN_RX_CMOS_ACL_OVR	0x89
#define B2056_SYN_LOGEN_TX_CMOS_ACL_OVR	0x8A
#define B2056_SYN_LOGEN_VCOBUF2		0x8B
#define B2056_SYN_LOGEN_MIXER3		0x8C
#define B2056_SYN_LOGEN_BUF5		0x8D
#define B2056_SYN_LOGEN_BUF6		0x8E
#define B2056_SYN_LOGEN_CBUFRX1		0x8F
#define B2056_SYN_LOGEN_CBUFRX2		0x90
#define B2056_SYN_LOGEN_CBUFRX3		0x91
#define B2056_SYN_LOGEN_CBUFRX4		0x92
#define B2056_SYN_LOGEN_CBUFTX1		0x93
#define B2056_SYN_LOGEN_CBUFTX2		0x94
#define B2056_SYN_LOGEN_CBUFTX3		0x95
#define B2056_SYN_LOGEN_CBUFTX4		0x96
#define B2056_SYN_LOGEN_CMOSRX1		0x97
#define B2056_SYN_LOGEN_CMOSRX2		0x98
#define B2056_SYN_LOGEN_CMOSRX3		0x99
#define B2056_SYN_LOGEN_CMOSRX4		0x9A
#define B2056_SYN_LOGEN_CMOSTX1		0x9B
#define B2056_SYN_LOGEN_CMOSTX2		0x9C
#define B2056_SYN_LOGEN_CMOSTX3		0x9D
#define B2056_SYN_LOGEN_CMOSTX4		0x9E
#define B2056_SYN_LOGEN_VCOBUF2_OVRVAL	0x9F
#define B2056_SYN_LOGEN_MIXER3_OVRVAL	0xA0
#define B2056_SYN_LOGEN_BUF5_OVRVAL	0xA1
#define B2056_SYN_LOGEN_BUF6_OVRVAL	0xA2
#define B2056_SYN_LOGEN_CBUFRX1_OVRVAL	0xA3
#define B2056_SYN_LOGEN_CBUFRX2_OVRVAL	0xA4
#define B2056_SYN_LOGEN_CBUFRX3_OVRVAL	0xA5
#define B2056_SYN_LOGEN_CBUFRX4_OVRVAL	0xA6
#define B2056_SYN_LOGEN_CBUFTX1_OVRVAL	0xA7
#define B2056_SYN_LOGEN_CBUFTX2_OVRVAL	0xA8
#define B2056_SYN_LOGEN_CBUFTX3_OVRVAL	0xA9
#define B2056_SYN_LOGEN_CBUFTX4_OVRVAL	0xAA
#define B2056_SYN_LOGEN_CMOSRX1_OVRVAL	0xAB
#define B2056_SYN_LOGEN_CMOSRX2_OVRVAL	0xAC
#define B2056_SYN_LOGEN_CMOSRX3_OVRVAL	0xAD
#define B2056_SYN_LOGEN_CMOSRX4_OVRVAL	0xAE
#define B2056_SYN_LOGEN_CMOSTX1_OVRVAL	0xAF
#define B2056_SYN_LOGEN_CMOSTX2_OVRVAL	0xB0
#define B2056_SYN_LOGEN_CMOSTX3_OVRVAL	0xB1
#define B2056_SYN_LOGEN_CMOSTX4_OVRVAL	0xB2
#define B2056_SYN_LOGEN_ACL_WAITCNT	0xB3
#define B2056_SYN_LOGEN_CORE_CALVALID	0xB4
#define B2056_SYN_LOGEN_RX_CMOS_CALVALID	0xB5
#define B2056_SYN_LOGEN_TX_CMOS_VALID	0xB6

#define B2056_TX_RESERVED_ADDR0		0x00
#define B2056_TX_IDCODE			0x01
#define B2056_TX_RESERVED_ADDR2		0x02
#define B2056_TX_RESERVED_ADDR3		0x03
#define B2056_TX_RESERVED_ADDR4		0x04
#define B2056_TX_RESERVED_ADDR5		0x05
#define B2056_TX_RESERVED_ADDR6		0x06
#define B2056_TX_RESERVED_ADDR7		0x07
#define B2056_TX_COM_CTRL		0x08
#define B2056_TX_COM_PU			0x09
#define B2056_TX_COM_OVR		0x0A
#define B2056_TX_COM_RESET		0x0B
#define B2056_TX_COM_RCAL		0x0C
#define B2056_TX_COM_RC_RXLPF		0x0D
#define B2056_TX_COM_RC_TXLPF		0x0E
#define B2056_TX_COM_RC_RXHPF		0x0F
#define B2056_TX_RESERVED_ADDR16	0x10
#define B2056_TX_RESERVED_ADDR17	0x11
#define B2056_TX_RESERVED_ADDR18	0x12
#define B2056_TX_RESERVED_ADDR19	0x13
#define B2056_TX_RESERVED_ADDR20	0x14
#define B2056_TX_RESERVED_ADDR21	0x15
#define B2056_TX_RESERVED_ADDR22	0x16
#define B2056_TX_RESERVED_ADDR23	0x17
#define B2056_TX_RESERVED_ADDR24	0x18
#define B2056_TX_RESERVED_ADDR25	0x19
#define B2056_TX_RESERVED_ADDR26	0x1A
#define B2056_TX_RESERVED_ADDR27	0x1B
#define B2056_TX_RESERVED_ADDR28	0x1C
#define B2056_TX_RESERVED_ADDR29	0x1D
#define B2056_TX_RESERVED_ADDR30	0x1E
#define B2056_TX_RESERVED_ADDR31	0x1F
#define B2056_TX_IQCAL_GAIN_BW		0x20
#define B2056_TX_LOFT_FINE_I		0x21
#define B2056_TX_LOFT_FINE_Q		0x22
#define B2056_TX_LOFT_COARSE_I		0x23
#define B2056_TX_LOFT_COARSE_Q		0x24
#define B2056_TX_TX_COM_MASTER1		0x25
#define B2056_TX_TX_COM_MASTER2		0x26
#define B2056_TX_RXIQCAL_TXMUX		0x27
#define B2056_TX_TX_SSI_MASTER		0x28
#define B2056_TX_IQCAL_VCM_HG		0x29
#define B2056_TX_IQCAL_IDAC		0x2A
#define B2056_TX_TSSI_VCM		0x2B
#define B2056_TX_TX_AMP_DET		0x2C
#define B2056_TX_TX_SSI_MUX		0x2D
#define B2056_TX_TSSIA			0x2E
#define B2056_TX_TSSIG			0x2F
#define B2056_TX_TSSI_MISC1		0x30
#define B2056_TX_TSSI_MISC2		0x31
#define B2056_TX_TSSI_MISC3		0x32
#define B2056_TX_PA_SPARE1		0x33
#define B2056_TX_PA_SPARE2		0x34
#define B2056_TX_INTPAA_MASTER		0x35
#define B2056_TX_INTPAA_GAIN		0x36
#define B2056_TX_INTPAA_BOOST_TUNE	0x37
#define B2056_TX_INTPAA_IAUX_STAT	0x38
#define B2056_TX_INTPAA_IAUX_DYN	0x39
#define B2056_TX_INTPAA_IMAIN_STAT	0x3A
#define B2056_TX_INTPAA_IMAIN_DYN	0x3B
#define B2056_TX_INTPAA_CASCBIAS	0x3C
#define B2056_TX_INTPAA_PASLOPE		0x3D
#define B2056_TX_INTPAA_PA_MISC		0x3E
#define B2056_TX_INTPAG_MASTER		0x3F
#define B2056_TX_INTPAG_GAIN		0x40
#define B2056_TX_INTPAG_BOOST_TUNE	0x41
#define B2056_TX_INTPAG_IAUX_STAT	0x42
#define B2056_TX_INTPAG_IAUX_DYN	0x43
#define B2056_TX_INTPAG_IMAIN_STAT	0x44
#define B2056_TX_INTPAG_IMAIN_DYN	0x45
#define B2056_TX_INTPAG_CASCBIAS	0x46
#define B2056_TX_INTPAG_PASLOPE		0x47
#define B2056_TX_INTPAG_PA_MISC		0x48
#define B2056_TX_PADA_MASTER		0x49
#define B2056_TX_PADA_IDAC		0x4A
#define B2056_TX_PADA_CASCBIAS		0x4B
#define B2056_TX_PADA_GAIN		0x4C
#define B2056_TX_PADA_BOOST_TUNE	0x4D
#define B2056_TX_PADA_SLOPE		0x4E
#define B2056_TX_PADG_MASTER		0x4F
#define B2056_TX_PADG_IDAC		0x50
#define B2056_TX_PADG_CASCBIAS		0x51
#define B2056_TX_PADG_GAIN		0x52
#define B2056_TX_PADG_BOOST_TUNE	0x53
#define B2056_TX_PADG_SLOPE		0x54
#define B2056_TX_PGAA_MASTER		0x55
#define B2056_TX_PGAA_IDAC		0x56
#define B2056_TX_PGAA_GAIN		0x57
#define B2056_TX_PGAA_BOOST_TUNE	0x58
#define B2056_TX_PGAA_SLOPE		0x59
#define B2056_TX_PGAA_MISC		0x5A
#define B2056_TX_PGAG_MASTER		0x5B
#define B2056_TX_PGAG_IDAC		0x5C
#define B2056_TX_PGAG_GAIN		0x5D
#define B2056_TX_PGAG_BOOST_TUNE	0x5E
#define B2056_TX_PGAG_SLOPE		0x5F
#define B2056_TX_PGAG_MISC		0x60
#define B2056_TX_MIXA_MASTER		0x61
#define B2056_TX_MIXA_BOOST_TUNE	0x62
#define B2056_TX_MIXG			0x63
#define B2056_TX_MIXG_BOOST_TUNE	0x64
#define B2056_TX_BB_GM_MASTER		0x65
#define B2056_TX_GMBB_GM		0x66
#define B2056_TX_GMBB_IDAC		0x67
#define B2056_TX_TXLPF_MASTER		0x68
#define B2056_TX_TXLPF_RCCAL		0x69
#define B2056_TX_TXLPF_RCCAL_OFF0	0x6A
#define B2056_TX_TXLPF_RCCAL_OFF1	0x6B
#define B2056_TX_TXLPF_RCCAL_OFF2	0x6C
#define B2056_TX_TXLPF_RCCAL_OFF3	0x6D
#define B2056_TX_TXLPF_RCCAL_OFF4	0x6E
#define B2056_TX_TXLPF_RCCAL_OFF5	0x6F
#define B2056_TX_TXLPF_RCCAL_OFF6	0x70
#define B2056_TX_TXLPF_BW		0x71
#define B2056_TX_TXLPF_GAIN		0x72
#define B2056_TX_TXLPF_IDAC		0x73
#define B2056_TX_TXLPF_IDAC_0		0x74
#define B2056_TX_TXLPF_IDAC_1		0x75
#define B2056_TX_TXLPF_IDAC_2		0x76
#define B2056_TX_TXLPF_IDAC_3		0x77
#define B2056_TX_TXLPF_IDAC_4		0x78
#define B2056_TX_TXLPF_IDAC_5		0x79
#define B2056_TX_TXLPF_IDAC_6		0x7A
#define B2056_TX_TXLPF_OPAMP_IDAC	0x7B
#define B2056_TX_TXLPF_MISC		0x7C
#define B2056_TX_TXSPARE1		0x7D
#define B2056_TX_TXSPARE2		0x7E
#define B2056_TX_TXSPARE3		0x7F
#define B2056_TX_TXSPARE4		0x80
#define B2056_TX_TXSPARE5		0x81
#define B2056_TX_TXSPARE6		0x82
#define B2056_TX_TXSPARE7		0x83
#define B2056_TX_TXSPARE8		0x84
#define B2056_TX_TXSPARE9		0x85
#define B2056_TX_TXSPARE10		0x86
#define B2056_TX_TXSPARE11		0x87
#define B2056_TX_TXSPARE12		0x88
#define B2056_TX_TXSPARE13		0x89
#define B2056_TX_TXSPARE14		0x8A
#define B2056_TX_TXSPARE15		0x8B
#define B2056_TX_TXSPARE16		0x8C
#define B2056_TX_STATUS_INTPA_GAIN	0x8D
#define B2056_TX_STATUS_PAD_GAIN	0x8E
#define B2056_TX_STATUS_PGA_GAIN	0x8F
#define B2056_TX_STATUS_GM_TXLPF_GAIN	0x90
#define B2056_TX_STATUS_TXLPF_BW	0x91
#define B2056_TX_STATUS_TXLPF_RC	0x92
#define B2056_TX_GMBB_IDAC0		0x93
#define B2056_TX_GMBB_IDAC1		0x94
#define B2056_TX_GMBB_IDAC2		0x95
#define B2056_TX_GMBB_IDAC3		0x96
#define B2056_TX_GMBB_IDAC4		0x97
#define B2056_TX_GMBB_IDAC5		0x98
#define B2056_TX_GMBB_IDAC6		0x99
#define B2056_TX_GMBB_IDAC7		0x9A

#define B2056_RX_RESERVED_ADDR0		0x00
#define B2056_RX_IDCODE			0x01
#define B2056_RX_RESERVED_ADDR2		0x02
#define B2056_RX_RESERVED_ADDR3		0x03
#define B2056_RX_RESERVED_ADDR4		0x04
#define B2056_RX_RESERVED_ADDR5		0x05
#define B2056_RX_RESERVED_ADDR6		0x06
#define B2056_RX_RESERVED_ADDR7		0x07
#define B2056_RX_COM_CTRL		0x08
#define B2056_RX_COM_PU			0x09
#define B2056_RX_COM_OVR		0x0A
#define B2056_RX_COM_RESET		0x0B
#define B2056_RX_COM_RCAL		0x0C
#define B2056_RX_COM_RC_RXLPF		0x0D
#define B2056_RX_COM_RC_TXLPF		0x0E
#define B2056_RX_COM_RC_RXHPF		0x0F
#define B2056_RX_RESERVED_ADDR16	0x10
#define B2056_RX_RESERVED_ADDR17	0x11
#define B2056_RX_RESERVED_ADDR18	0x12
#define B2056_RX_RESERVED_ADDR19	0x13
#define B2056_RX_RESERVED_ADDR20	0x14
#define B2056_RX_RESERVED_ADDR21	0x15
#define B2056_RX_RESERVED_ADDR22	0x16
#define B2056_RX_RESERVED_ADDR23	0x17
#define B2056_RX_RESERVED_ADDR24	0x18
#define B2056_RX_RESERVED_ADDR25	0x19
#define B2056_RX_RESERVED_ADDR26	0x1A
#define B2056_RX_RESERVED_ADDR27	0x1B
#define B2056_RX_RESERVED_ADDR28	0x1C
#define B2056_RX_RESERVED_ADDR29	0x1D
#define B2056_RX_RESERVED_ADDR30	0x1E
#define B2056_RX_RESERVED_ADDR31	0x1F
#define B2056_RX_RXIQCAL_RXMUX		0x20
#define B2056_RX_RSSI_PU		0x21
#define B2056_RX_RSSI_SEL		0x22
#define B2056_RX_RSSI_GAIN		0x23
#define B2056_RX_RSSI_NB_IDAC		0x24
#define B2056_RX_RSSI_WB2I_IDAC_1	0x25
#define B2056_RX_RSSI_WB2I_IDAC_2	0x26
#define B2056_RX_RSSI_WB2Q_IDAC_1	0x27
#define B2056_RX_RSSI_WB2Q_IDAC_2	0x28
#define B2056_RX_RSSI_POLE		0x29
#define B2056_RX_RSSI_WB1_IDAC		0x2A
#define B2056_RX_RSSI_MISC		0x2B
#define B2056_RX_LNAA_MASTER		0x2C
#define B2056_RX_LNAA_TUNE		0x2D
#define B2056_RX_LNAA_GAIN		0x2E
#define B2056_RX_LNA_A_SLOPE		0x2F
#define B2056_RX_BIASPOLE_LNAA1_IDAC	0x30
#define B2056_RX_LNAA2_IDAC		0x31
#define B2056_RX_LNA1A_MISC		0x32
#define B2056_RX_LNAG_MASTER		0x33
#define B2056_RX_LNAG_TUNE		0x34
#define B2056_RX_LNAG_GAIN		0x35
#define B2056_RX_LNA_G_SLOPE		0x36
#define B2056_RX_BIASPOLE_LNAG1_IDAC	0x37
#define B2056_RX_LNAG2_IDAC		0x38
#define B2056_RX_LNA1G_MISC		0x39
#define B2056_RX_MIXA_MASTER		0x3A
#define B2056_RX_MIXA_VCM		0x3B
#define B2056_RX_MIXA_CTRLPTAT		0x3C
#define B2056_RX_MIXA_LOB_BIAS		0x3D
#define B2056_RX_MIXA_CORE_IDAC		0x3E
#define B2056_RX_MIXA_CMFB_IDAC		0x3F
#define B2056_RX_MIXA_BIAS_AUX		0x40
#define B2056_RX_MIXA_BIAS_MAIN		0x41
#define B2056_RX_MIXA_BIAS_MISC		0x42
#define B2056_RX_MIXA_MAST_BIAS		0x43
#define B2056_RX_MIXG_MASTER		0x44
#define B2056_RX_MIXG_VCM		0x45
#define B2056_RX_MIXG_CTRLPTAT		0x46
#define B2056_RX_MIXG_LOB_BIAS		0x47
#define B2056_RX_MIXG_CORE_IDAC		0x48
#define B2056_RX_MIXG_CMFB_IDAC		0x49
#define B2056_RX_MIXG_BIAS_AUX		0x4A
#define B2056_RX_MIXG_BIAS_MAIN		0x4B
#define B2056_RX_MIXG_BIAS_MISC		0x4C
#define B2056_RX_MIXG_MAST_BIAS		0x4D
#define B2056_RX_TIA_MASTER		0x4E
#define B2056_RX_TIA_IOPAMP		0x4F
#define B2056_RX_TIA_QOPAMP		0x50
#define B2056_RX_TIA_IMISC		0x51
#define B2056_RX_TIA_QMISC		0x52
#define B2056_RX_TIA_GAIN		0x53
#define B2056_RX_TIA_SPARE1		0x54
#define B2056_RX_TIA_SPARE2		0x55
#define B2056_RX_BB_LPF_MASTER		0x56
#define B2056_RX_AACI_MASTER		0x57
#define B2056_RX_RXLPF_IDAC		0x58
#define B2056_RX_RXLPF_OPAMPBIAS_LOWQ	0x59
#define B2056_RX_RXLPF_OPAMPBIAS_HIGHQ	0x5A
#define B2056_RX_RXLPF_BIAS_DCCANCEL	0x5B
#define B2056_RX_RXLPF_OUTVCM		0x5C
#define B2056_RX_RXLPF_INVCM_BODY	0x5D
#define B2056_RX_RXLPF_CC_OP		0x5E
#define B2056_RX_RXLPF_GAIN		0x5F
#define B2056_RX_RXLPF_Q_BW		0x60
#define B2056_RX_RXLPF_HP_CORNER_BW	0x61
#define B2056_RX_RXLPF_RCCAL_HPC	0x62
#define B2056_RX_RXHPF_OFF0		0x63
#define B2056_RX_RXHPF_OFF1		0x64
#define B2056_RX_RXHPF_OFF2		0x65
#define B2056_RX_RXHPF_OFF3		0x66
#define B2056_RX_RXHPF_OFF4		0x67
#define B2056_RX_RXHPF_OFF5		0x68
#define B2056_RX_RXHPF_OFF6		0x69
#define B2056_RX_RXHPF_OFF7		0x6A
#define B2056_RX_RXLPF_RCCAL_LPC	0x6B
#define B2056_RX_RXLPF_OFF_0		0x6C
#define B2056_RX_RXLPF_OFF_1		0x6D
#define B2056_RX_RXLPF_OFF_2		0x6E
#define B2056_RX_RXLPF_OFF_3		0x6F
#define B2056_RX_RXLPF_OFF_4		0x70
#define B2056_RX_UNUSED			0x71
#define B2056_RX_VGA_MASTER		0x72
#define B2056_RX_VGA_BIAS		0x73
#define B2056_RX_VGA_BIAS_DCCANCEL	0x74
#define B2056_RX_VGA_GAIN		0x75
#define B2056_RX_VGA_HP_CORNER_BW	0x76
#define B2056_RX_VGABUF_BIAS		0x77
#define B2056_RX_VGABUF_GAIN_BW		0x78
#define B2056_RX_TXFBMIX_A		0x79
#define B2056_RX_TXFBMIX_G		0x7A
#define B2056_RX_RXSPARE1		0x7B
#define B2056_RX_RXSPARE2		0x7C
#define B2056_RX_RXSPARE3		0x7D
#define B2056_RX_RXSPARE4		0x7E
#define B2056_RX_RXSPARE5		0x7F
#define B2056_RX_RXSPARE6		0x80
#define B2056_RX_RXSPARE7		0x81
#define B2056_RX_RXSPARE8		0x82
#define B2056_RX_RXSPARE9		0x83
#define B2056_RX_RXSPARE10		0x84
#define B2056_RX_RXSPARE11		0x85
#define B2056_RX_RXSPARE12		0x86
#define B2056_RX_RXSPARE13		0x87
#define B2056_RX_RXSPARE14		0x88
#define B2056_RX_RXSPARE15		0x89
#define B2056_RX_RXSPARE16		0x8A
#define B2056_RX_STATUS_LNAA_GAIN	0x8B
#define B2056_RX_STATUS_LNAG_GAIN	0x8C
#define B2056_RX_STATUS_MIXTIA_GAIN	0x8D
#define B2056_RX_STATUS_RXLPF_GAIN	0x8E
#define B2056_RX_STATUS_VGA_BUF_GAIN	0x8F
#define B2056_RX_STATUS_RXLPF_Q		0x90
#define B2056_RX_STATUS_RXLPF_BUF_BW	0x91
#define B2056_RX_STATUS_RXLPF_VGA_HPC	0x92
#define B2056_RX_STATUS_RXLPF_RC	0x93
#define B2056_RX_STATUS_HPC_RC		0x94

#define B2056_LNA1_A_PU			0x01
#define B2056_LNA2_A_PU			0x02
#define B2056_LNA1_G_PU			0x01
#define B2056_LNA2_G_PU			0x02
#define B2056_MIXA_PU_I			0x01
#define B2056_MIXA_PU_Q			0x02
#define B2056_MIXA_PU_GM		0x10
#define B2056_MIXG_PU_I			0x01
#define B2056_MIXG_PU_Q			0x02
#define B2056_MIXG_PU_GM		0x10
#define B2056_TIA_PU			0x01
#define B2056_BB_LPF_PU			0x20
#define B2056_W1_PU			0x02
#define B2056_W2_PU			0x04
#define B2056_NB_PU			0x08
#define B2056_RSSI_W1_SEL		0x02
#define B2056_RSSI_W2_SEL		0x04
#define B2056_RSSI_NB_SEL		0x08
#define B2056_VCM_MASK			0x1C
#define B2056_RSSI_VCM_SHIFT		0x02

struct b43_nphy_channeltab_entry_rev3 {
	/* The channel frequency in MHz */
	u16 freq;
	/* Radio register values on channelswitch */
	u8 radio_syn_pll_vcocal1;
	u8 radio_syn_pll_vcocal2;
	u8 radio_syn_pll_refdiv;
	u8 radio_syn_pll_mmd2;
	u8 radio_syn_pll_mmd1;
	u8 radio_syn_pll_loopfilter1;
	u8 radio_syn_pll_loopfilter2;
	u8 radio_syn_pll_loopfilter3;
	u8 radio_syn_pll_loopfilter4;
	u8 radio_syn_pll_loopfilter5;
	u8 radio_syn_reserved_addr27;
	u8 radio_syn_reserved_addr28;
	u8 radio_syn_reserved_addr29;
	u8 radio_syn_logen_vcobuf1;
	u8 radio_syn_logen_mixer2;
	u8 radio_syn_logen_buf3;
	u8 radio_syn_logen_buf4;
	u8 radio_rx0_lnaa_tune;
	u8 radio_rx0_lnag_tune;
	u8 radio_tx0_intpaa_boost_tune;
	u8 radio_tx0_intpag_boost_tune;
	u8 radio_tx0_pada_boost_tune;
	u8 radio_tx0_padg_boost_tune;
	u8 radio_tx0_pgaa_boost_tune;
	u8 radio_tx0_pgag_boost_tune;
	u8 radio_tx0_mixa_boost_tune;
	u8 radio_tx0_mixg_boost_tune;
	u8 radio_rx1_lnaa_tune;
	u8 radio_rx1_lnag_tune;
	u8 radio_tx1_intpaa_boost_tune;
	u8 radio_tx1_intpag_boost_tune;
	u8 radio_tx1_pada_boost_tune;
	u8 radio_tx1_padg_boost_tune;
	u8 radio_tx1_pgaa_boost_tune;
	u8 radio_tx1_pgag_boost_tune;
	u8 radio_tx1_mixa_boost_tune;
	u8 radio_tx1_mixg_boost_tune;
	/* PHY register values on channelswitch */
	struct b43_phy_n_sfo_cfg phy_regs;
};

void b2056_upload_inittabs(struct b43_wldev *dev,
			   bool ghz5, bool ignore_uploadflag);

#endif /* B43_RADIO_2056_H_ */
