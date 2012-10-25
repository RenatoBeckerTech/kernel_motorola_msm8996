#ifndef _LINUX_MIGRATE_H
#define _LINUX_MIGRATE_H

#include <linux/mm.h>
#include <linux/mempolicy.h>
#include <linux/migrate_mode.h>

typedef struct page *new_page_t(struct page *, unsigned long private, int **);

enum migrate_reason {
	MR_COMPACTION,
	MR_MEMORY_FAILURE,
	MR_MEMORY_HOTPLUG,
	MR_SYSCALL,		/* also applies to cpusets */
	MR_MEMPOLICY_MBIND,
	MR_NUMA_MISPLACED,
	MR_CMA
};

#ifdef CONFIG_MIGRATION

extern void putback_lru_pages(struct list_head *l);
extern int migrate_page(struct address_space *,
			struct page *, struct page *, enum migrate_mode);
extern int migrate_pages(struct list_head *l, new_page_t x,
			unsigned long private, bool offlining,
			enum migrate_mode mode, int reason);
extern int migrate_huge_page(struct page *, new_page_t x,
			unsigned long private, bool offlining,
			enum migrate_mode mode);

extern int fail_migrate_page(struct address_space *,
			struct page *, struct page *);

extern int migrate_prep(void);
extern int migrate_prep_local(void);
extern int migrate_vmas(struct mm_struct *mm,
		const nodemask_t *from, const nodemask_t *to,
		unsigned long flags);
extern void migrate_page_copy(struct page *newpage, struct page *page);
extern int migrate_huge_page_move_mapping(struct address_space *mapping,
				  struct page *newpage, struct page *page);
#else

static inline void putback_lru_pages(struct list_head *l) {}
static inline int migrate_pages(struct list_head *l, new_page_t x,
		unsigned long private, bool offlining,
		enum migrate_mode mode, int reason) { return -ENOSYS; }
static inline int migrate_huge_page(struct page *page, new_page_t x,
		unsigned long private, bool offlining,
		enum migrate_mode mode) { return -ENOSYS; }

static inline int migrate_prep(void) { return -ENOSYS; }
static inline int migrate_prep_local(void) { return -ENOSYS; }

static inline int migrate_vmas(struct mm_struct *mm,
		const nodemask_t *from, const nodemask_t *to,
		unsigned long flags)
{
	return -ENOSYS;
}

static inline void migrate_page_copy(struct page *newpage,
				     struct page *page) {}

static inline int migrate_huge_page_move_mapping(struct address_space *mapping,
				  struct page *newpage, struct page *page)
{
	return -ENOSYS;
}

/* Possible settings for the migrate_page() method in address_operations */
#define migrate_page NULL
#define fail_migrate_page NULL

#endif /* CONFIG_MIGRATION */

#ifdef CONFIG_NUMA_BALANCING
extern int migrate_misplaced_page(struct page *page, int node);
#else
static inline int migrate_misplaced_page(struct page *page, int node)
{
	return -EAGAIN; /* can't migrate now */
}
#endif /* CONFIG_NUMA_BALANCING */

#endif /* _LINUX_MIGRATE_H */
