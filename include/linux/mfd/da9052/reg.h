/*
 * Register declarations for DA9052 PMICs.
 *
 * Copyright(c) 2011 Dialog Semiconductor Ltd.
 *
 * Author: David Dajun Chen <dchen@diasemi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 */

#ifndef __LINUX_MFD_DA9052_REG_H
#define __LINUX_MFD_DA9052_REG_H

/* PAGE REGISTERS */
#define DA9052_PAGE0_CON_REG		0
#define DA9052_PAGE1_CON_REG		128

/* STATUS REGISTERS */
#define DA9052_STATUS_A_REG		1
#define DA9052_STATUS_B_REG		2
#define DA9052_STATUS_C_REG		3
#define DA9052_STATUS_D_REG		4

/* PARK REGISTER */
#define DA9052_PARK_REGISTER		DA9052_STATUS_D_REG

/* EVENT REGISTERS */
#define DA9052_EVENT_A_REG		5
#define DA9052_EVENT_B_REG		6
#define DA9052_EVENT_C_REG		7
#define DA9052_EVENT_D_REG		8
#define DA9052_FAULTLOG_REG		9

/* IRQ REGISTERS */
#define DA9052_IRQ_MASK_A_REG		10
#define DA9052_IRQ_MASK_B_REG		11
#define DA9052_IRQ_MASK_C_REG		12
#define DA9052_IRQ_MASK_D_REG		13

/* CONTROL REGISTERS */
#define DA9052_CONTROL_A_REG		14
#define DA9052_CONTROL_B_REG		15
#define DA9052_CONTROL_C_REG		16
#define DA9052_CONTROL_D_REG		17

#define DA9052_PDDIS_REG		18
#define DA9052_INTERFACE_REG		19
#define DA9052_RESET_REG		20

/* GPIO REGISTERS */
#define DA9052_GPIO_0_1_REG		21
#define DA9052_GPIO_2_3_REG		22
#define DA9052_GPIO_4_5_REG		23
#define DA9052_GPIO_6_7_REG		24
#define DA9052_GPIO_14_15_REG		28

/* POWER SEQUENCER CONTROL REGISTERS */
#define DA9052_ID_0_1_REG		29
#define DA9052_ID_2_3_REG		30
#define DA9052_ID_4_5_REG		31
#define DA9052_ID_6_7_REG		32
#define DA9052_ID_8_9_REG		33
#define DA9052_ID_10_11_REG		34
#define DA9052_ID_12_13_REG		35
#define DA9052_ID_14_15_REG		36
#define DA9052_ID_16_17_REG		37
#define DA9052_ID_18_19_REG		38
#define DA9052_ID_20_21_REG		39
#define DA9052_SEQ_STATUS_REG		40
#define DA9052_SEQ_A_REG		41
#define DA9052_SEQ_B_REG		42
#define DA9052_SEQ_TIMER_REG		43

/* LDO AND BUCK REGISTERS */
#define DA9052_BUCKA_REG		44
#define DA9052_BUCKB_REG		45
#define DA9052_BUCKCORE_REG		46
#define DA9052_BUCKPRO_REG		47
#define DA9052_BUCKMEM_REG		48
#define DA9052_BUCKPERI_REG		49
#define DA9052_LDO1_REG		50
#define DA9052_LDO2_REG		51
#define DA9052_LDO3_REG		52
#define DA9052_LDO4_REG		53
#define DA9052_LDO5_REG		54
#define DA9052_LDO6_REG		55
#define DA9052_LDO7_REG		56
#define DA9052_LDO8_REG		57
#define DA9052_LDO9_REG		58
#define DA9052_LDO10_REG		59
#define DA9052_SUPPLY_REG		60
#define DA9052_PULLDOWN_REG		61
#define DA9052_CHGBUCK_REG		62
#define DA9052_WAITCONT_REG		63
#define DA9052_ISET_REG		64
#define DA9052_BATCHG_REG		65

/* BATTERY CONTROL REGISTRS */
#define DA9052_CHG_CONT_REG		66
#define DA9052_INPUT_CONT_REG		67
#define DA9052_CHG_TIME_REG		68
#define DA9052_BBAT_CONT_REG		69

/* LED CONTROL REGISTERS */
#define DA9052_BOOST_REG		70
#define DA9052_LED_CONT_REG		71
#define DA9052_LEDMIN123_REG		72
#define DA9052_LED1_CONF_REG		73
#define DA9052_LED2_CONF_REG		74
#define DA9052_LED3_CONF_REG		75
#define DA9052_LED1CONT_REG		76
#define DA9052_LED2CONT_REG		77
#define DA9052_LED3CONT_REG		78
#define DA9052_LED_CONT_4_REG		79
#define DA9052_LED_CONT_5_REG		80

/* ADC CONTROL REGISTERS */
#define DA9052_ADC_MAN_REG		81
#define DA9052_ADC_CONT_REG		82
#define DA9052_ADC_RES_L_REG		83
#define DA9052_ADC_RES_H_REG		84
#define DA9052_VDD_RES_REG		85
#define DA9052_VDD_MON_REG		86

#define DA9052_ICHG_AV_REG		87
#define DA9052_ICHG_THD_REG		88
#define DA9052_ICHG_END_REG		89
#define DA9052_TBAT_RES_REG		90
#define DA9052_TBAT_HIGHP_REG		91
#define DA9052_TBAT_HIGHN_REG		92
#define DA9052_TBAT_LOW_REG		93
#define DA9052_T_OFFSET_REG		94

#define DA9052_ADCIN4_RES_REG		95
#define DA9052_AUTO4_HIGH_REG		96
#define DA9052_AUTO4_LOW_REG		97
#define DA9052_ADCIN5_RES_REG		98
#define DA9052_AUTO5_HIGH_REG		99
#define DA9052_AUTO5_LOW_REG		100
#define DA9052_ADCIN6_RES_REG		101
#define DA9052_AUTO6_HIGH_REG		102
#define DA9052_AUTO6_LOW_REG		103

#define DA9052_TJUNC_RES_REG		104

/* TSI CONTROL REGISTERS */
#define DA9052_TSI_CONT_A_REG		105
#define DA9052_TSI_CONT_B_REG		106
#define DA9052_TSI_X_MSB_REG		107
#define DA9052_TSI_Y_MSB_REG		108
#define DA9052_TSI_LSB_REG		109
#define DA9052_TSI_Z_MSB_REG		110

/* RTC COUNT REGISTERS */
#define DA9052_COUNT_S_REG		111
#define DA9052_COUNT_MI_REG		112
#define DA9052_COUNT_H_REG		113
#define DA9052_COUNT_D_REG		114
#define DA9052_COUNT_MO_REG		115
#define DA9052_COUNT_Y_REG		116

/* RTC CONTROL REGISTERS */
#define DA9052_ALARM_MI_REG		117
#define DA9052_ALARM_H_REG		118
#define DA9052_ALARM_D_REG		119
#define DA9052_ALARM_MO_REG		120
#define DA9052_ALARM_Y_REG		121
#define DA9052_SECOND_A_REG		122
#define DA9052_SECOND_B_REG		123
#define DA9052_SECOND_C_REG		124
#define DA9052_SECOND_D_REG		125

/* PAGE CONFIGURATION BIT */
#define DA9052_PAGE_CONF		0X80

/* STATUS REGISTER A BITS */
#define DA9052_STATUSA_VDATDET		0X80
#define DA9052_STATUSA_VBUSSEL		0X40
#define DA9052_STATUSA_DCINSEL		0X20
#define DA9052_STATUSA_VBUSDET		0X10
#define DA9052_STATUSA_DCINDET		0X08
#define DA9052_STATUSA_IDGND		0X04
#define DA9052_STATUSA_IDFLOAT		0X02
#define DA9052_STATUSA_NONKEY		0X01

/* STATUS REGISTER B BITS */
#define DA9052_STATUSB_COMPDET		0X80
#define DA9052_STATUSB_SEQUENCING	0X40
#define DA9052_STATUSB_GPFB2		0X20
#define DA9052_STATUSB_CHGTO		0X10
#define DA9052_STATUSB_CHGEND		0X08
#define DA9052_STATUSB_CHGLIM		0X04
#define DA9052_STATUSB_CHGPRE		0X02
#define DA9052_STATUSB_CHGATT		0X01

/* STATUS REGISTER C BITS */
#define DA9052_STATUSC_GPI7		0X80
#define DA9052_STATUSC_GPI6		0X40
#define DA9052_STATUSC_GPI5		0X20
#define DA9052_STATUSC_GPI4		0X10
#define DA9052_STATUSC_GPI3		0X08
#define DA9052_STATUSC_GPI2		0X04
#define DA9052_STATUSC_GPI1		0X02
#define DA9052_STATUSC_GPI0		0X01

/* STATUS REGISTER D BITS */
#define DA9052_STATUSD_GPI15		0X80
#define DA9052_STATUSD_GPI14		0X40
#define DA9052_STATUSD_GPI13		0X20
#define DA9052_STATUSD_GPI12		0X10
#define DA9052_STATUSD_GPI11		0X08
#define DA9052_STATUSD_GPI10		0X04
#define DA9052_STATUSD_GPI9		0X02
#define DA9052_STATUSD_GPI8		0X01

/* EVENT REGISTER A BITS */
#define DA9052_EVENTA_ECOMP1V2		0X80
#define DA9052_EVENTA_ESEQRDY		0X40
#define DA9052_EVENTA_EALRAM		0X20
#define DA9052_EVENTA_EVDDLOW		0X10
#define DA9052_EVENTA_EVBUSREM		0X08
#define DA9052_EVENTA_EDCINREM		0X04
#define DA9052_EVENTA_EVBUSDET		0X02
#define DA9052_EVENTA_EDCINDET		0X01

/* EVENT REGISTER B BITS */
#define DA9052_EVENTB_ETSIREADY	0X80
#define DA9052_EVENTB_EPENDOWN		0X40
#define DA9052_EVENTB_EADCEOM		0X20
#define DA9052_EVENTB_ETBAT		0X10
#define DA9052_EVENTB_ECHGEND		0X08
#define DA9052_EVENTB_EIDGND		0X04
#define DA9052_EVENTB_EIDFLOAT		0X02
#define DA9052_EVENTB_ENONKEY		0X01

/* EVENT REGISTER C BITS */
#define DA9052_EVENTC_EGPI7		0X80
#define DA9052_EVENTC_EGPI6		0X40
#define DA9052_EVENTC_EGPI5		0X20
#define DA9052_EVENTC_EGPI4		0X10
#define DA9052_EVENTC_EGPI3		0X08
#define DA9052_EVENTC_EGPI2		0X04
#define DA9052_EVENTC_EGPI1		0X02
#define DA9052_EVENTC_EGPI0		0X01

/* EVENT REGISTER D BITS */
#define DA9052_EVENTD_EGPI15		0X80
#define DA9052_EVENTD_EGPI14		0X40
#define DA9052_EVENTD_EGPI13		0X20
#define DA9052_EVENTD_EGPI12		0X10
#define DA9052_EVENTD_EGPI11		0X08
#define DA9052_EVENTD_EGPI10		0X04
#define DA9052_EVENTD_EGPI9		0X02
#define DA9052_EVENTD_EGPI8		0X01

/* IRQ MASK REGISTERS BITS */
#define DA9052_M_NONKEY		0X0100

/* TSI EVENT REGISTERS BITS */
#define DA9052_E_PEN_DOWN		0X4000
#define DA9052_E_TSI_READY		0X8000

/* FAULT LOG REGISTER BITS */
#define DA9052_FAULTLOG_WAITSET	0X80
#define DA9052_FAULTLOG_NSDSET		0X40
#define DA9052_FAULTLOG_KEYSHUT	0X20
#define DA9052_FAULTLOG_TEMPOVER	0X08
#define DA9052_FAULTLOG_VDDSTART	0X04
#define DA9052_FAULTLOG_VDDFAULT	0X02
#define DA9052_FAULTLOG_TWDERROR	0X01

/* CONTROL REGISTER A BITS */
#define DA9052_CONTROLA_GPIV		0X80
#define DA9052_CONTROLA_PMOTYPE	0X20
#define DA9052_CONTROLA_PMOV		0X10
#define DA9052_CONTROLA_PMIV		0X08
#define DA9052_CONTROLA_PMIFV		0X08
#define DA9052_CONTROLA_PWR1EN		0X04
#define DA9052_CONTROLA_PWREN		0X02
#define DA9052_CONTROLA_SYSEN		0X01

/* CONTROL REGISTER B BITS */
#define DA9052_CONTROLB_SHUTDOWN	0X80
#define DA9052_CONTROLB_DEEPSLEEP	0X40
#define DA9052_CONTROL_B_WRITEMODE	0X20
#define DA9052_CONTROLB_BBATEN		0X10
#define DA9052_CONTROLB_OTPREADEN	0X08
#define DA9052_CONTROLB_AUTOBOOT	0X04
#define DA9052_CONTROLB_ACTDIODE	0X02
#define DA9052_CONTROLB_BUCKMERGE	0X01

/* CONTROL REGISTER C BITS */
#define DA9052_CONTROLC_BLINKDUR	0X80
#define DA9052_CONTROLC_BLINKFRQ	0X60
#define DA9052_CONTROLC_DEBOUNCING	0X1C
#define DA9052_CONTROLC_PMFB2PIN	0X02
#define DA9052_CONTROLC_PMFB1PIN	0X01

/* CONTROL REGISTER D BITS */
#define DA9052_CONTROLD_WATCHDOG	0X80
#define DA9052_CONTROLD_ACCDETEN	0X40
#define DA9052_CONTROLD_GPI1415SD	0X20
#define DA9052_CONTROLD_NONKEYSD	0X10
#define DA9052_CONTROLD_KEEPACTEN	0X08
#define DA9052_CONTROLD_TWDSCALE	0X07

/* POWER DOWN DISABLE REGISTER BITS */
#define DA9052_PDDIS_PMCONTPD		0X80
#define DA9052_PDDIS_OUT32KPD		0X40
#define DA9052_PDDIS_CHGBBATPD		0X20
#define DA9052_PDDIS_CHGPD		0X10
#define DA9052_PDDIS_HS2WIREPD		0X08
#define DA9052_PDDIS_PMIFPD		0X04
#define DA9052_PDDIS_GPADCPD		0X02
#define DA9052_PDDIS_GPIOPD		0X01

/* CONTROL REGISTER D BITS */
#define DA9052_INTERFACE_IFBASEADDR	0XE0
#define DA9052_INTERFACE_NCSPOL	0X10
#define DA9052_INTERFACE_RWPOL		0X08
#define DA9052_INTERFACE_CPHA		0X04
#define DA9052_INTERFACE_CPOL		0X02
#define DA9052_INTERFACE_IFTYPE	0X01

/* CONTROL REGISTER D BITS */
#define DA9052_RESET_RESETEVENT	0XC0
#define DA9052_RESET_RESETTIMER	0X3F

/* GPIO REGISTERS */
/* GPIO CONTROL REGISTER BITS */
#define DA9052_GPIO_EVEN_PORT_PIN	0X03
#define DA9052_GPIO_EVEN_PORT_TYPE	0X04
#define DA9052_GPIO_EVEN_PORT_MODE	0X08

#define DA9052_GPIO_ODD_PORT_PIN	0X30
#define DA9052_GPIO_ODD_PORT_TYPE	0X40
#define DA9052_GPIO_ODD_PORT_MODE	0X80

/*POWER SEQUENCER REGISTER BITS */
/* SEQ CONTROL REGISTER BITS FOR ID 0 AND 1 */
#define DA9052_ID01_LDO1STEP		0XF0
#define DA9052_ID01_SYSPRE		0X04
#define DA9052_ID01_DEFSUPPLY		0X02
#define DA9052_ID01_NRESMODE		0X01

/* SEQ CONTROL REGISTER BITS FOR ID 2 AND 3 */
#define DA9052_ID23_LDO3STEP		0XF0
#define DA9052_ID23_LDO2STEP		0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 4 AND 5 */
#define DA9052_ID45_LDO5STEP		0XF0
#define DA9052_ID45_LDO4STEP		0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 6 AND 7 */
#define DA9052_ID67_LDO7STEP		0XF0
#define DA9052_ID67_LDO6STEP		0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 8 AND 9 */
#define DA9052_ID89_LDO9STEP		0XF0
#define DA9052_ID89_LDO8STEP		0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 10 AND 11 */
#define DA9052_ID1011_PDDISSTEP	0XF0
#define DA9052_ID1011_LDO10STEP	0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 12 AND 13 */
#define DA9052_ID1213_VMEMSWSTEP	0XF0
#define DA9052_ID1213_VPERISWSTEP	0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 14 AND 15 */
#define DA9052_ID1415_BUCKPROSTEP	0XF0
#define DA9052_ID1415_BUCKCORESTEP	0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 16 AND 17 */
#define DA9052_ID1617_BUCKPERISTEP	0XF0
#define DA9052_ID1617_BUCKMEMSTEP	0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 18 AND 19 */
#define DA9052_ID1819_GPRISE2STEP	0XF0
#define DA9052_ID1819_GPRISE1STEP	0X0F

/* SEQ CONTROL REGISTER BITS FOR ID 20 AND 21 */
#define DA9052_ID2021_GPFALL2STEP	0XF0
#define DA9052_ID2021_GPFALL1STEP	0X0F

/* POWER SEQ STATUS REGISTER BITS */
#define DA9052_SEQSTATUS_SEQPOINTER	0XF0
#define DA9052_SEQSTATUS_WAITSTEP	0X0F

/* POWER SEQ A REGISTER BITS */
#define DA9052_SEQA_POWEREND		0XF0
#define DA9052_SEQA_SYSTEMEND		0X0F

/* POWER SEQ B REGISTER BITS */
#define DA9052_SEQB_PARTDOWN		0XF0
#define DA9052_SEQB_MAXCOUNT		0X0F

/* POWER SEQ TIMER REGISTER BITS */
#define DA9052_SEQTIMER_SEQDUMMY	0XF0
#define DA9052_SEQTIMER_SEQTIME	0X0F

/*POWER SUPPLY CONTROL REGISTER BITS */
/* BUCK REGISTER A BITS */
#define DA9052_BUCKA_BPROILIM		0XC0
#define DA9052_BUCKA_BPROMODE		0X30
#define DA9052_BUCKA_BCOREILIM		0X0C
#define DA9052_BUCKA_BCOREMODE		0X03

/* BUCK REGISTER B BITS */
#define DA9052_BUCKB_BERIILIM		0XC0
#define DA9052_BUCKB_BPERIMODE		0X30
#define DA9052_BUCKB_BMEMILIM		0X0C
#define DA9052_BUCKB_BMEMMODE		0X03

/* BUCKCORE REGISTER BITS */
#define DA9052_BUCKCORE_BCORECONF	0X80
#define DA9052_BUCKCORE_BCOREEN	0X40
#define DA9052_BUCKCORE_VBCORE		0X3F

/* BUCKPRO REGISTER BITS */
#define DA9052_BUCKPRO_BPROCONF	0X80
#define DA9052_BUCKPRO_BPROEN		0X40
#define DA9052_BUCKPRO_VBPRO		0X3F

/* BUCKMEM REGISTER BITS */
#define DA9052_BUCKMEM_BMEMCONF	0X80
#define DA9052_BUCKMEM_BMEMEN		0X40
#define DA9052_BUCKMEM_VBMEM		0X3F

/* BUCKPERI REGISTER BITS */
#define DA9052_BUCKPERI_BPERICONF	0X80
#define DA9052_BUCKPERI_BPERIEN	0X40
#define DA9052_BUCKPERI_BPERIHS	0X20
#define DA9052_BUCKPERI_VBPERI		0X1F

/* LDO1 REGISTER BITS */
#define DA9052_LDO1_LDO1CONF		0X80
#define DA9052_LDO1_LDO1EN		0X40
#define DA9052_LDO1_VLDO1		0X1F

/* LDO2 REGISTER BITS */
#define DA9052_LDO2_LDO2CONF		0X80
#define DA9052_LDO2_LDO2EN		0X40
#define DA9052_LDO2_VLDO2		0X3F

/* LDO3 REGISTER BITS */
#define DA9052_LDO3_LDO3CONF		0X80
#define DA9052_LDO3_LDO3EN		0X40
#define DA9052_LDO3_VLDO3		0X3F

/* LDO4 REGISTER BITS */
#define DA9052_LDO4_LDO4CONF		0X80
#define DA9052_LDO4_LDO4EN		0X40
#define DA9052_LDO4_VLDO4		0X3F

/* LDO5 REGISTER BITS */
#define DA9052_LDO5_LDO5CONF		0X80
#define DA9052_LDO5_LDO5EN		0X40
#define DA9052_LDO5_VLDO5		0X3F

/* LDO6 REGISTER BITS */
#define DA9052_LDO6_LDO6CONF		0X80
#define DA9052_LDO6_LDO6EN		0X40
#define DA9052_LDO6_VLDO6		0X3F

/* LDO7 REGISTER BITS */
#define DA9052_LDO7_LDO7CONF		0X80
#define DA9052_LDO7_LDO7EN		0X40
#define DA9052_LDO7_VLDO7		0X3F

/* LDO8 REGISTER BITS */
#define DA9052_LDO8_LDO8CONF		0X80
#define DA9052_LDO8_LDO8EN		0X40
#define DA9052_LDO8_VLDO8		0X3F

/* LDO9 REGISTER BITS */
#define DA9052_LDO9_LDO9CONF		0X80
#define DA9052_LDO9_LDO9EN		0X40
#define DA9052_LDO9_VLDO9		0X3F

/* LDO10 REGISTER BITS */
#define DA9052_LDO10_LDO10CONF		0X80
#define DA9052_LDO10_LDO10EN		0X40
#define DA9052_LDO10_VLDO10		0X3F

/* SUPPLY REGISTER BITS */
#define DA9052_SUPPLY_VLOCK		0X80
#define DA9052_SUPPLY_VMEMSWEN		0X40
#define DA9052_SUPPLY_VPERISWEN	0X20
#define DA9052_SUPPLY_VLDO3GO		0X10
#define DA9052_SUPPLY_VLDO2GO		0X08
#define DA9052_SUPPLY_VBMEMGO		0X04
#define DA9052_SUPPLY_VBPROGO		0X02
#define DA9052_SUPPLY_VBCOREGO		0X01

/* PULLDOWN REGISTER BITS */
#define DA9052_PULLDOWN_LDO5PDDIS	0X20
#define DA9052_PULLDOWN_LDO2PDDIS	0X10
#define DA9052_PULLDOWN_LDO1PDDIS	0X08
#define DA9052_PULLDOWN_MEMPDDIS	0X04
#define DA9052_PULLDOWN_PROPDDIS	0X02
#define DA9052_PULLDOWN_COREPDDIS	0X01

/* BAT CHARGER REGISTER BITS */
/* CHARGER BUCK REGISTER BITS */
#define DA9052_CHGBUCK_CHGTEMP		0X80
#define DA9052_CHGBUCK_CHGUSBILIM	0X40
#define DA9052_CHGBUCK_CHGBUCKLP	0X20
#define DA9052_CHGBUCK_CHGBUCKEN	0X10
#define DA9052_CHGBUCK_ISETBUCK	0X0F

/* WAIT COUNTER REGISTER BITS */
#define DA9052_WAITCONT_WAITDIR	0X80
#define DA9052_WAITCONT_RTCCLOCK	0X40
#define DA9052_WAITCONT_WAITMODE	0X20
#define DA9052_WAITCONT_EN32KOUT	0X10
#define DA9052_WAITCONT_DELAYTIME	0X0F

/* ISET CONTROL REGISTER BITS */
#define DA9052_ISET_ISETDCIN		0XF0
#define DA9052_ISET_ISETVBUS		0X0F

/* BATTERY CHARGER CONTROL REGISTER BITS */
#define DA9052_BATCHG_ICHGPRE		0XC0
#define DA9052_BATCHG_ICHGBAT		0X3F

/* CHARGER COUNTER REGISTER BITS */
#define DA9052_CHG_CONT_VCHG_BAT	0XF8
#define DA9052_CHG_CONT_TCTR		0X07

/* INPUT CONTROL REGISTER BITS */
#define DA9052_INPUT_CONT_TCTR_MODE	0X80
#define DA9052_INPUT_CONT_VBUS_SUSP	0X10
#define DA9052_INPUT_CONT_DCIN_SUSP	0X08

/* CHARGING TIME REGISTER BITS */
#define DA9052_CHGTIME_CHGTIME		0XFF

/* BACKUP BATTERY CONTROL REGISTER BITS */
#define DA9052_BBATCONT_BCHARGERISET	0XF0
#define DA9052_BBATCONT_BCHARGERVSET	0X0F

/* LED REGISTERS BITS */
/* LED BOOST REGISTER BITS */
#define DA9052_BOOST_EBFAULT		0X80
#define DA9052_BOOST_MBFAULT		0X40
#define DA9052_BOOST_BOOSTFRQ		0X20
#define DA9052_BOOST_BOOSTILIM		0X10
#define DA9052_BOOST_LED3INEN		0X08
#define DA9052_BOOST_LED2INEN		0X04
#define DA9052_BOOST_LED1INEN		0X02
#define DA9052_BOOST_BOOSTEN		0X01

/* LED CONTROL REGISTER BITS */
#define DA9052_LEDCONT_SELLEDMODE	0X80
#define DA9052_LEDCONT_LED3ICONT	0X40
#define DA9052_LEDCONT_LED3RAMP	0X20
#define DA9052_LEDCONT_LED3EN		0X10
#define DA9052_LEDCONT_LED2RAMP	0X08
#define DA9052_LEDCONT_LED2EN		0X04
#define DA9052_LEDCONT_LED1RAMP	0X02
#define DA9052_LEDCONT_LED1EN		0X01

/* LEDMIN123 REGISTER BIT */
#define DA9052_LEDMIN123_LEDMINCURRENT	0XFF

/* LED1CONF REGISTER BIT */
#define DA9052_LED1CONF_LED1CURRENT	0XFF

/* LED2CONF REGISTER BIT */
#define DA9052_LED2CONF_LED2CURRENT	0XFF

/* LED3CONF REGISTER BIT */
#define DA9052_LED3CONF_LED3CURRENT	0XFF

/* LED COUNT REGISTER BIT */
#define DA9052_LED_CONT_DIM		0X80

/* ADC MAN REGISTERS BITS */
#define DA9052_ADC_MAN_MAN_CONV	0X10
#define DA9052_ADC_MAN_MUXSEL_VDDOUT	0X00
#define DA9052_ADC_MAN_MUXSEL_ICH	0X01
#define DA9052_ADC_MAN_MUXSEL_TBAT	0X02
#define DA9052_ADC_MAN_MUXSEL_VBAT	0X03
#define DA9052_ADC_MAN_MUXSEL_AD4	0X04
#define DA9052_ADC_MAN_MUXSEL_AD5	0X05
#define DA9052_ADC_MAN_MUXSEL_AD6	0X06
#define DA9052_ADC_MAN_MUXSEL_VBBAT	0X09

/* ADC CONTROL REGSISTERS BITS */
#define DA9052_ADCCONT_COMP1V2EN	0X80
#define DA9052_ADCCONT_ADCMODE		0X40
#define DA9052_ADCCONT_TBATISRCEN	0X20
#define DA9052_ADCCONT_AD4ISRCEN	0X10
#define DA9052_ADCCONT_AUTOAD6EN	0X08
#define DA9052_ADCCONT_AUTOAD5EN	0X04
#define DA9052_ADCCONT_AUTOAD4EN	0X02
#define DA9052_ADCCONT_AUTOVDDEN	0X01

/* ADC 10 BIT MANUAL CONVERSION RESULT LOW REGISTER */
#define DA9052_ADC_RES_LSB		0X03

/* ADC 10 BIT MANUAL CONVERSION RESULT HIGH REGISTER */
#define DA9052_ADCRESH_ADCRESMSB	0XFF

/* VDD RES REGSISTER BIT*/
#define DA9052_VDDRES_VDDOUTRES	0XFF

/* VDD MON REGSISTER BIT */
#define DA9052_VDDMON_VDDOUTMON	0XFF

/* ICHG_AV REGSISTER BIT */
#define DA9052_ICHGAV_ICHGAV		0XFF

/* ICHG_THD REGSISTER BIT */
#define DA9052_ICHGTHD_ICHGTHD		0XFF

/* ICHG_END REGSISTER BIT */
#define DA9052_ICHGEND_ICHGEND		0XFF

/* TBAT_RES REGSISTER BIT */
#define DA9052_TBATRES_TBATRES		0XFF

/* TBAT_HIGHP REGSISTER BIT */
#define DA9052_TBATHIGHP_TBATHIGHP	0XFF

/* TBAT_HIGHN REGSISTER BIT */
#define DA9052_TBATHIGHN_TBATHIGHN	0XFF

/* TBAT_LOW REGSISTER BIT */
#define DA9052_TBATLOW_TBATLOW		0XFF

/* T_OFFSET REGSISTER BIT */
#define DA9052_TOFFSET_TOFFSET		0XFF

/* ADCIN4_RES REGSISTER BIT */
#define DA9052_ADCIN4RES_ADCIN4RES	0XFF

/* ADCIN4_HIGH REGSISTER BIT */
#define DA9052_AUTO4HIGH_AUTO4HIGH	0XFF

/* ADCIN4_LOW REGSISTER BIT */
#define DA9052_AUTO4LOW_AUTO4LOW	0XFF

/* ADCIN5_RES REGSISTER BIT */
#define DA9052_ADCIN5RES_ADCIN5RES	0XFF

/* ADCIN5_HIGH REGSISTER BIT */
#define DA9052_AUTO5HIGH_AUTOHIGH	0XFF

/* ADCIN5_LOW REGSISTER BIT */
#define DA9052_AUTO5LOW_AUTO5LOW	0XFF

/* ADCIN6_RES REGSISTER BIT */
#define DA9052_ADCIN6RES_ADCIN6RES	0XFF

/* ADCIN6_HIGH REGSISTER BIT */
#define DA9052_AUTO6HIGH_AUTO6HIGH	0XFF

/* ADCIN6_LOW REGSISTER BIT */
#define DA9052_AUTO6LOW_AUTO6LOW	0XFF

/* TJUNC_RES REGSISTER BIT*/
#define DA9052_TJUNCRES_TJUNCRES	0XFF

/* TSI REGISTER */
/* TSI CONTROL REGISTER A BITS */
#define DA9052_TSICONTA_TSIDELAY	0XC0
#define DA9052_TSICONTA_TSISKIP	0X38
#define DA9052_TSICONTA_TSIMODE	0X04
#define DA9052_TSICONTA_PENDETEN	0X02
#define DA9052_TSICONTA_AUTOTSIEN	0X01

/* TSI CONTROL REGISTER B BITS */
#define DA9052_TSICONTB_ADCREF		0X80
#define DA9052_TSICONTB_TSIMAN		0X40
#define DA9052_TSICONTB_TSIMUX		0X30
#define DA9052_TSICONTB_TSISEL3	0X08
#define DA9052_TSICONTB_TSISEL2	0X04
#define DA9052_TSICONTB_TSISEL1	0X02
#define DA9052_TSICONTB_TSISEL0	0X01

/* TSI X CO-ORDINATE MSB RESULT REGISTER BITS */
#define DA9052_TSIXMSB_TSIXM		0XFF

/* TSI Y CO-ORDINATE MSB RESULT REGISTER BITS */
#define DA9052_TSIYMSB_TSIYM		0XFF

/* TSI CO-ORDINATE LSB RESULT REGISTER BITS */
#define DA9052_TSILSB_PENDOWN		0X40
#define DA9052_TSILSB_TSIZL		0X30
#define DA9052_TSILSB_TSIYL		0X0C
#define DA9052_TSILSB_TSIXL		0X03

/* TSI Z MEASUREMENT MSB RESULT REGISTER BIT */
#define DA9052_TSIZMSB_TSIZM		0XFF

/* RTC REGISTER */
/* RTC TIMER SECONDS REGISTER BITS */
#define DA9052_COUNTS_MONITOR		0X40
#define DA9052_RTC_SEC			0X3F

/* RTC TIMER MINUTES REGISTER BIT */
#define DA9052_RTC_MIN			0X3F

/* RTC TIMER HOUR REGISTER BIT */
#define DA9052_RTC_HOUR		0X1F

/* RTC TIMER DAYS REGISTER BIT */
#define DA9052_RTC_DAY			0X1F

/* RTC TIMER MONTHS REGISTER BIT */
#define DA9052_RTC_MONTH		0X0F

/* RTC TIMER YEARS REGISTER BIT */
#define DA9052_RTC_YEAR		0X3F

/* RTC ALARM MINUTES REGISTER BITS */
#define DA9052_ALARMM_I_TICK_TYPE	0X80
#define DA9052_ALARMMI_ALARMTYPE	0X40

/* RTC ALARM YEARS REGISTER BITS */
#define DA9052_ALARM_Y_TICK_ON		0X80
#define DA9052_ALARM_Y_ALARM_ON	0X40

/* RTC SECONDS REGISTER A BITS */
#define DA9052_SECONDA_SECONDSA	0XFF

/* RTC SECONDS REGISTER B BITS */
#define DA9052_SECONDB_SECONDSB	0XFF

/* RTC SECONDS REGISTER C BITS */
#define DA9052_SECONDC_SECONDSC	0XFF

/* RTC SECONDS REGISTER D BITS */
#define DA9052_SECONDD_SECONDSD	0XFF

#endif
/* __LINUX_MFD_DA9052_REG_H */
