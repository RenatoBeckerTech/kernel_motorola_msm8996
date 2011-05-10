#ifndef __ASM_MACH_PXA168_H
#define __ASM_MACH_PXA168_H

struct sys_timer;

extern struct sys_timer pxa168_timer;
extern void __init pxa168_init_irq(void);
extern void pxa168_clear_keypad_wakeup(void);

#include <linux/i2c.h>
#include <linux/i2c/pxa-i2c.h>
#include <mach/devices.h>
#include <plat/pxa3xx_nand.h>
#include <video/pxa168fb.h>
#include <plat/pxa27x_keypad.h>
#include <mach/cputype.h>

extern struct pxa_device_desc pxa168_device_uart1;
extern struct pxa_device_desc pxa168_device_uart2;
extern struct pxa_device_desc pxa168_device_twsi0;
extern struct pxa_device_desc pxa168_device_twsi1;
extern struct pxa_device_desc pxa168_device_pwm1;
extern struct pxa_device_desc pxa168_device_pwm2;
extern struct pxa_device_desc pxa168_device_pwm3;
extern struct pxa_device_desc pxa168_device_pwm4;
extern struct pxa_device_desc pxa168_device_ssp1;
extern struct pxa_device_desc pxa168_device_ssp2;
extern struct pxa_device_desc pxa168_device_ssp3;
extern struct pxa_device_desc pxa168_device_ssp4;
extern struct pxa_device_desc pxa168_device_ssp5;
extern struct pxa_device_desc pxa168_device_nand;
extern struct pxa_device_desc pxa168_device_fb;
extern struct pxa_device_desc pxa168_device_keypad;

static inline int pxa168_add_uart(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa168_device_uart1; break;
	case 2: d = &pxa168_device_uart2; break;
	}

	if (d == NULL)
		return -EINVAL;

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa168_add_twsi(int id, struct i2c_pxa_platform_data *data,
				  struct i2c_board_info *info, unsigned size)
{
	struct pxa_device_desc *d = NULL;
	int ret;

	switch (id) {
	case 0: d = &pxa168_device_twsi0; break;
	case 1: d = &pxa168_device_twsi1; break;
	default:
		return -EINVAL;
	}

	ret = i2c_register_board_info(id, info, size);
	if (ret)
		return ret;

	return pxa_register_device(d, data, sizeof(*data));
}

static inline int pxa168_add_pwm(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa168_device_pwm1; break;
	case 2: d = &pxa168_device_pwm2; break;
	case 3: d = &pxa168_device_pwm3; break;
	case 4: d = &pxa168_device_pwm4; break;
	default:
		return -EINVAL;
	}

	return pxa_register_device(d, NULL, 0);
}

static inline int pxa168_add_ssp(int id)
{
	struct pxa_device_desc *d = NULL;

	switch (id) {
	case 1: d = &pxa168_device_ssp1; break;
	case 2: d = &pxa168_device_ssp2; break;
	case 3: d = &pxa168_device_ssp3; break;
	case 4: d = &pxa168_device_ssp4; break;
	case 5: d = &pxa168_device_ssp5; break;
	default:
		return -EINVAL;
	}
	return pxa_register_device(d, NULL, 0);
}

static inline int pxa168_add_nand(struct pxa3xx_nand_platform_data *info)
{
	return pxa_register_device(&pxa168_device_nand, info, sizeof(*info));
}

static inline int pxa168_add_fb(struct pxa168fb_mach_info *mi)
{
	return pxa_register_device(&pxa168_device_fb, mi, sizeof(*mi));
}

static inline int pxa168_add_keypad(struct pxa27x_keypad_platform_data *data)
{
	if (cpu_is_pxa168())
		data->clear_wakeup_event = pxa168_clear_keypad_wakeup;

	return pxa_register_device(&pxa168_device_keypad, data, sizeof(*data));
}

#endif /* __ASM_MACH_PXA168_H */
