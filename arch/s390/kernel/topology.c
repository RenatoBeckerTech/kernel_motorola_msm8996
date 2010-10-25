/*
 *    Copyright IBM Corp. 2007
 *    Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>
 */

#define KMSG_COMPONENT "cpu"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/bootmem.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/cpuset.h>
#include <asm/delay.h>
#include <asm/s390_ext.h>
#include <asm/sysinfo.h>

#define CPU_BITS 64
#define NR_MAG 6

#define PTF_HORIZONTAL	(0UL)
#define PTF_VERTICAL	(1UL)
#define PTF_CHECK	(2UL)

struct tl_cpu {
	unsigned char reserved0[4];
	unsigned char :6;
	unsigned char pp:2;
	unsigned char reserved1;
	unsigned short origin;
	unsigned long mask[CPU_BITS / BITS_PER_LONG];
};

struct tl_container {
	unsigned char reserved[7];
	unsigned char id;
};

union tl_entry {
	unsigned char nl;
	struct tl_cpu cpu;
	struct tl_container container;
};

struct tl_info {
	unsigned char reserved0[2];
	unsigned short length;
	unsigned char mag[NR_MAG];
	unsigned char reserved1;
	unsigned char mnest;
	unsigned char reserved2[4];
	union tl_entry tle[0];
};

struct mask_info {
	struct mask_info *next;
	unsigned char id;
	cpumask_t mask;
};

static int topology_enabled = 1;
static void topology_work_fn(struct work_struct *work);
static struct tl_info *tl_info;
static struct timer_list topology_timer;
static void set_topology_timer(void);
static DECLARE_WORK(topology_work, topology_work_fn);
/* topology_lock protects the core linked list */
static DEFINE_SPINLOCK(topology_lock);

static struct mask_info core_info;
cpumask_t cpu_core_map[NR_CPUS];
unsigned char cpu_core_id[NR_CPUS];

#ifdef CONFIG_SCHED_BOOK
static struct mask_info book_info;
cpumask_t cpu_book_map[NR_CPUS];
unsigned char cpu_book_id[NR_CPUS];
#endif

static cpumask_t cpu_group_map(struct mask_info *info, unsigned int cpu)
{
	cpumask_t mask;

	cpus_clear(mask);
	if (!topology_enabled || !MACHINE_HAS_TOPOLOGY)
		return cpu_possible_map;
	while (info) {
		if (cpu_isset(cpu, info->mask)) {
			mask = info->mask;
			break;
		}
		info = info->next;
	}
	if (cpus_empty(mask))
		mask = cpumask_of_cpu(cpu);
	return mask;
}

static void add_cpus_to_mask(struct tl_cpu *tl_cpu, struct mask_info *book,
			     struct mask_info *core)
{
	unsigned int cpu;

	for (cpu = find_first_bit(&tl_cpu->mask[0], CPU_BITS);
	     cpu < CPU_BITS;
	     cpu = find_next_bit(&tl_cpu->mask[0], CPU_BITS, cpu + 1))
	{
		unsigned int rcpu, lcpu;

		rcpu = CPU_BITS - 1 - cpu + tl_cpu->origin;
		for_each_present_cpu(lcpu) {
			if (cpu_logical_map(lcpu) != rcpu)
				continue;
#ifdef CONFIG_SCHED_BOOK
			cpu_set(lcpu, book->mask);
			cpu_book_id[lcpu] = book->id;
#endif
			cpu_set(lcpu, core->mask);
			cpu_core_id[lcpu] = core->id;
			smp_cpu_polarization[lcpu] = tl_cpu->pp;
		}
	}
}

static void clear_masks(void)
{
	struct mask_info *info;

	info = &core_info;
	while (info) {
		cpus_clear(info->mask);
		info = info->next;
	}
#ifdef CONFIG_SCHED_BOOK
	info = &book_info;
	while (info) {
		cpus_clear(info->mask);
		info = info->next;
	}
#endif
}

static union tl_entry *next_tle(union tl_entry *tle)
{
	if (tle->nl)
		return (union tl_entry *)((struct tl_container *)tle + 1);
	else
		return (union tl_entry *)((struct tl_cpu *)tle + 1);
}

static void tl_to_cores(struct tl_info *info)
{
#ifdef CONFIG_SCHED_BOOK
	struct mask_info *book = &book_info;
#else
	struct mask_info *book = NULL;
#endif
	struct mask_info *core = &core_info;
	union tl_entry *tle, *end;


	spin_lock_irq(&topology_lock);
	clear_masks();
	tle = info->tle;
	end = (union tl_entry *)((unsigned long)info + info->length);
	while (tle < end) {
		switch (tle->nl) {
#ifdef CONFIG_SCHED_BOOK
		case 2:
			book = book->next;
			book->id = tle->container.id;
			break;
#endif
		case 1:
			core = core->next;
			core->id = tle->container.id;
			break;
		case 0:
			add_cpus_to_mask(&tle->cpu, book, core);
			break;
		default:
			clear_masks();
			goto out;
		}
		tle = next_tle(tle);
	}
out:
	spin_unlock_irq(&topology_lock);
}

static void topology_update_polarization_simple(void)
{
	int cpu;

	mutex_lock(&smp_cpu_state_mutex);
	for_each_possible_cpu(cpu)
		smp_cpu_polarization[cpu] = POLARIZATION_HRZ;
	mutex_unlock(&smp_cpu_state_mutex);
}

static int ptf(unsigned long fc)
{
	int rc;

	asm volatile(
		"	.insn	rre,0xb9a20000,%1,%1\n"
		"	ipm	%0\n"
		"	srl	%0,28\n"
		: "=d" (rc)
		: "d" (fc)  : "cc");
	return rc;
}

int topology_set_cpu_management(int fc)
{
	int cpu;
	int rc;

	if (!MACHINE_HAS_TOPOLOGY)
		return -EOPNOTSUPP;
	if (fc)
		rc = ptf(PTF_VERTICAL);
	else
		rc = ptf(PTF_HORIZONTAL);
	if (rc)
		return -EBUSY;
	for_each_possible_cpu(cpu)
		smp_cpu_polarization[cpu] = POLARIZATION_UNKNWN;
	return rc;
}

static void update_cpu_core_map(void)
{
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&topology_lock, flags);
	for_each_possible_cpu(cpu) {
		cpu_core_map[cpu] = cpu_group_map(&core_info, cpu);
#ifdef CONFIG_SCHED_BOOK
		cpu_book_map[cpu] = cpu_group_map(&book_info, cpu);
#endif
	}
	spin_unlock_irqrestore(&topology_lock, flags);
}

static void store_topology(struct tl_info *info)
{
#ifdef CONFIG_SCHED_BOOK
	int rc;

	rc = stsi(info, 15, 1, 3);
	if (rc != -ENOSYS)
		return;
#endif
	stsi(info, 15, 1, 2);
}

int arch_update_cpu_topology(void)
{
	struct tl_info *info = tl_info;
	struct sys_device *sysdev;
	int cpu;

	if (!MACHINE_HAS_TOPOLOGY) {
		update_cpu_core_map();
		topology_update_polarization_simple();
		return 0;
	}
	store_topology(info);
	tl_to_cores(info);
	update_cpu_core_map();
	for_each_online_cpu(cpu) {
		sysdev = get_cpu_sysdev(cpu);
		kobject_uevent(&sysdev->kobj, KOBJ_CHANGE);
	}
	return 1;
}

static void topology_work_fn(struct work_struct *work)
{
	rebuild_sched_domains();
}

void topology_schedule_update(void)
{
	schedule_work(&topology_work);
}

static void topology_timer_fn(unsigned long ignored)
{
	if (ptf(PTF_CHECK))
		topology_schedule_update();
	set_topology_timer();
}

static void set_topology_timer(void)
{
	topology_timer.function = topology_timer_fn;
	topology_timer.data = 0;
	topology_timer.expires = jiffies + 60 * HZ;
	add_timer(&topology_timer);
}

static int __init early_parse_topology(char *p)
{
	if (strncmp(p, "off", 3))
		return 0;
	topology_enabled = 0;
	return 0;
}
early_param("topology", early_parse_topology);

static int __init init_topology_update(void)
{
	int rc;

	rc = 0;
	if (!MACHINE_HAS_TOPOLOGY) {
		topology_update_polarization_simple();
		goto out;
	}
	init_timer_deferrable(&topology_timer);
	set_topology_timer();
out:
	update_cpu_core_map();
	return rc;
}
__initcall(init_topology_update);

static void alloc_masks(struct tl_info *info, struct mask_info *mask, int offset)
{
	int i, nr_masks;

	nr_masks = info->mag[NR_MAG - offset];
	for (i = 0; i < info->mnest - offset; i++)
		nr_masks *= info->mag[NR_MAG - offset - 1 - i];
	nr_masks = max(nr_masks, 1);
	for (i = 0; i < nr_masks; i++) {
		mask->next = alloc_bootmem(sizeof(struct mask_info));
		mask = mask->next;
	}
}

void __init s390_init_cpu_topology(void)
{
	struct tl_info *info;
	int i;

	if (!MACHINE_HAS_TOPOLOGY)
		return;
	tl_info = alloc_bootmem_pages(PAGE_SIZE);
	info = tl_info;
	store_topology(info);
	pr_info("The CPU configuration topology of the machine is:");
	for (i = 0; i < NR_MAG; i++)
		printk(" %d", info->mag[i]);
	printk(" / %d\n", info->mnest);
	alloc_masks(info, &core_info, 2);
#ifdef CONFIG_SCHED_BOOK
	alloc_masks(info, &book_info, 3);
#endif
}
