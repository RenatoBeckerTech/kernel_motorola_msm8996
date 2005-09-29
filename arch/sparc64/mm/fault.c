/* $Id: fault.c,v 1.59 2002/02/09 19:49:31 davem Exp $
 * arch/sparc64/mm/fault.c: Page fault handlers for the 64-bit Sparc.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include <asm/head.h>

#include <linux/string.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kprobes.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/uaccess.h>
#include <asm/asi.h>
#include <asm/lsu.h>
#include <asm/sections.h>
#include <asm/kdebug.h>

#define ELEMENTS(arr) (sizeof (arr)/sizeof (arr[0]))

extern struct sparc_phys_banks sp_banks[SPARC_PHYS_BANKS];

/*
 * To debug kernel to catch accesses to certain virtual/physical addresses.
 * Mode = 0 selects physical watchpoints, mode = 1 selects virtual watchpoints.
 * flags = VM_READ watches memread accesses, flags = VM_WRITE watches memwrite accesses.
 * Caller passes in a 64bit aligned addr, with mask set to the bytes that need to be
 * watched. This is only useful on a single cpu machine for now. After the watchpoint
 * is detected, the process causing it will be killed, thus preventing an infinite loop.
 */
void set_brkpt(unsigned long addr, unsigned char mask, int flags, int mode)
{
	unsigned long lsubits;

	__asm__ __volatile__("ldxa [%%g0] %1, %0"
			     : "=r" (lsubits)
			     : "i" (ASI_LSU_CONTROL));
	lsubits &= ~(LSU_CONTROL_PM | LSU_CONTROL_VM |
		     LSU_CONTROL_PR | LSU_CONTROL_VR |
		     LSU_CONTROL_PW | LSU_CONTROL_VW);

	__asm__ __volatile__("stxa	%0, [%1] %2\n\t"
			     "membar	#Sync"
			     : /* no outputs */
			     : "r" (addr), "r" (mode ? VIRT_WATCHPOINT : PHYS_WATCHPOINT),
			       "i" (ASI_DMMU));

	lsubits |= ((unsigned long)mask << (mode ? 25 : 33));
	if (flags & VM_READ)
		lsubits |= (mode ? LSU_CONTROL_VR : LSU_CONTROL_PR);
	if (flags & VM_WRITE)
		lsubits |= (mode ? LSU_CONTROL_VW : LSU_CONTROL_PW);
	__asm__ __volatile__("stxa %0, [%%g0] %1\n\t"
			     "membar #Sync"
			     : /* no outputs */
			     : "r" (lsubits), "i" (ASI_LSU_CONTROL)
			     : "memory");
}

/* Nice, simple, prom library does all the sweating for us. ;) */
unsigned long __init prom_probe_memory (void)
{
	register struct linux_mlist_p1275 *mlist;
	register unsigned long bytes, base_paddr, tally;
	register int i;

	i = 0;
	mlist = *prom_meminfo()->p1275_available;
	bytes = tally = mlist->num_bytes;
	base_paddr = mlist->start_adr;
  
	sp_banks[0].base_addr = base_paddr;
	sp_banks[0].num_bytes = bytes;

	while (mlist->theres_more != (void *) 0) {
		i++;
		mlist = mlist->theres_more;
		bytes = mlist->num_bytes;
		tally += bytes;
		if (i >= SPARC_PHYS_BANKS-1) {
			printk ("The machine has more banks than "
				"this kernel can support\n"
				"Increase the SPARC_PHYS_BANKS "
				"setting (currently %d)\n",
				SPARC_PHYS_BANKS);
			i = SPARC_PHYS_BANKS-1;
			break;
		}
    
		sp_banks[i].base_addr = mlist->start_adr;
		sp_banks[i].num_bytes = mlist->num_bytes;
	}

	i++;
	sp_banks[i].base_addr = 0xdeadbeefbeefdeadUL;
	sp_banks[i].num_bytes = 0;

	/* Now mask all bank sizes on a page boundary, it is all we can
	 * use anyways.
	 */
	for (i = 0; sp_banks[i].num_bytes != 0; i++)
		sp_banks[i].num_bytes &= PAGE_MASK;

	return tally;
}

static void __kprobes unhandled_fault(unsigned long address,
				      struct task_struct *tsk,
				      struct pt_regs *regs)
{
	if ((unsigned long) address < PAGE_SIZE) {
		printk(KERN_ALERT "Unable to handle kernel NULL "
		       "pointer dereference\n");
	} else {
		printk(KERN_ALERT "Unable to handle kernel paging request "
		       "at virtual address %016lx\n", (unsigned long)address);
	}
	printk(KERN_ALERT "tsk->{mm,active_mm}->context = %016lx\n",
	       (tsk->mm ?
		CTX_HWBITS(tsk->mm->context) :
		CTX_HWBITS(tsk->active_mm->context)));
	printk(KERN_ALERT "tsk->{mm,active_mm}->pgd = %016lx\n",
	       (tsk->mm ? (unsigned long) tsk->mm->pgd :
		          (unsigned long) tsk->active_mm->pgd));
	if (notify_die(DIE_GPF, "general protection fault", regs,
		       0, 0, SIGSEGV) == NOTIFY_STOP)
		return;
	die_if_kernel("Oops", regs);
}

static void bad_kernel_pc(struct pt_regs *regs)
{
	unsigned long *ksp;

	printk(KERN_CRIT "OOPS: Bogus kernel PC [%016lx] in fault handler\n",
	       regs->tpc);
	__asm__("mov %%sp, %0" : "=r" (ksp));
	show_stack(current, ksp);
	unhandled_fault(regs->tpc, current, regs);
}

/*
 * We now make sure that mmap_sem is held in all paths that call 
 * this. Additionally, to prevent kswapd from ripping ptes from
 * under us, raise interrupts around the time that we look at the
 * pte, kswapd will have to wait to get his smp ipi response from
 * us. This saves us having to get page_table_lock.
 */
static unsigned int get_user_insn(unsigned long tpc)
{
	pgd_t *pgdp = pgd_offset(current->mm, tpc);
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep, pte;
	unsigned long pa;
	u32 insn = 0;
	unsigned long pstate;

	if (pgd_none(*pgdp))
		goto outret;
	pudp = pud_offset(pgdp, tpc);
	if (pud_none(*pudp))
		goto outret;
	pmdp = pmd_offset(pudp, tpc);
	if (pmd_none(*pmdp))
		goto outret;

	/* This disables preemption for us as well. */
	__asm__ __volatile__("rdpr %%pstate, %0" : "=r" (pstate));
	__asm__ __volatile__("wrpr %0, %1, %%pstate"
				: : "r" (pstate), "i" (PSTATE_IE));
	ptep = pte_offset_map(pmdp, tpc);
	pte = *ptep;
	if (!pte_present(pte))
		goto out;

	pa  = (pte_val(pte) & _PAGE_PADDR);
	pa += (tpc & ~PAGE_MASK);

	/* Use phys bypass so we don't pollute dtlb/dcache. */
	__asm__ __volatile__("lduwa [%1] %2, %0"
			     : "=r" (insn)
			     : "r" (pa), "i" (ASI_PHYS_USE_EC));

out:
	pte_unmap(ptep);
	__asm__ __volatile__("wrpr %0, 0x0, %%pstate" : : "r" (pstate));
outret:
	return insn;
}

extern unsigned long compute_effective_address(struct pt_regs *, unsigned int, unsigned int);

static void do_fault_siginfo(int code, int sig, struct pt_regs *regs,
			     unsigned int insn, int fault_code)
{
	siginfo_t info;

	info.si_code = code;
	info.si_signo = sig;
	info.si_errno = 0;
	if (fault_code & FAULT_CODE_ITLB)
		info.si_addr = (void __user *) regs->tpc;
	else
		info.si_addr = (void __user *)
			compute_effective_address(regs, insn, 0);
	info.si_trapno = 0;
	force_sig_info(sig, &info, current);
}

extern int handle_ldf_stq(u32, struct pt_regs *);
extern int handle_ld_nf(u32, struct pt_regs *);

static unsigned int get_fault_insn(struct pt_regs *regs, unsigned int insn)
{
	if (!insn) {
		if (!regs->tpc || (regs->tpc & 0x3))
			return 0;
		if (regs->tstate & TSTATE_PRIV) {
			insn = *(unsigned int *) regs->tpc;
		} else {
			insn = get_user_insn(regs->tpc);
		}
	}
	return insn;
}

static void do_kernel_fault(struct pt_regs *regs, int si_code, int fault_code,
			    unsigned int insn, unsigned long address)
{
	unsigned char asi = ASI_P;
 
	if ((!insn) && (regs->tstate & TSTATE_PRIV))
		goto cannot_handle;

	/* If user insn could be read (thus insn is zero), that
	 * is fine.  We will just gun down the process with a signal
	 * in that case.
	 */

	if (!(fault_code & (FAULT_CODE_WRITE|FAULT_CODE_ITLB)) &&
	    (insn & 0xc0800000) == 0xc0800000) {
		if (insn & 0x2000)
			asi = (regs->tstate >> 24);
		else
			asi = (insn >> 5);
		if ((asi & 0xf2) == 0x82) {
			if (insn & 0x1000000) {
				handle_ldf_stq(insn, regs);
			} else {
				/* This was a non-faulting load. Just clear the
				 * destination register(s) and continue with the next
				 * instruction. -jj
				 */
				handle_ld_nf(insn, regs);
			}
			return;
		}
	}
		
	/* Is this in ex_table? */
	if (regs->tstate & TSTATE_PRIV) {
		const struct exception_table_entry *entry;

		if (asi == ASI_P && (insn & 0xc0800000) == 0xc0800000) {
			if (insn & 0x2000)
				asi = (regs->tstate >> 24);
			else
				asi = (insn >> 5);
		}
	
		/* Look in asi.h: All _S asis have LS bit set */
		if ((asi & 0x1) &&
		    (entry = search_exception_tables(regs->tpc))) {
			regs->tpc = entry->fixup;
			regs->tnpc = regs->tpc + 4;
			return;
		}
	} else {
		/* The si_code was set to make clear whether
		 * this was a SEGV_MAPERR or SEGV_ACCERR fault.
		 */
		do_fault_siginfo(si_code, SIGSEGV, regs, insn, fault_code);
		return;
	}

cannot_handle:
	unhandled_fault (address, current, regs);
}

asmlinkage void __kprobes do_sparc64_fault(struct pt_regs *regs)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned int insn = 0;
	int si_code, fault_code;
	unsigned long address;

	fault_code = get_thread_fault_code();

	if (notify_die(DIE_PAGE_FAULT, "page_fault", regs,
		       fault_code, 0, SIGSEGV) == NOTIFY_STOP)
		return;

	si_code = SEGV_MAPERR;
	address = current_thread_info()->fault_address;

	if ((fault_code & FAULT_CODE_ITLB) &&
	    (fault_code & FAULT_CODE_DTLB))
		BUG();

	if (regs->tstate & TSTATE_PRIV) {
		unsigned long tpc = regs->tpc;

		/* Sanity check the PC. */
		if ((tpc >= KERNBASE && tpc < (unsigned long) _etext) ||
		    (tpc >= MODULES_VADDR && tpc < MODULES_END)) {
			/* Valid, no problems... */
		} else {
			bad_kernel_pc(regs);
			return;
		}
	}

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_atomic() || !mm)
		goto intr_or_no_mm;

	if (test_thread_flag(TIF_32BIT)) {
		if (!(regs->tstate & TSTATE_PRIV))
			regs->tpc &= 0xffffffff;
		address &= 0xffffffff;
	}

	if (!down_read_trylock(&mm->mmap_sem)) {
		if ((regs->tstate & TSTATE_PRIV) &&
		    !search_exception_tables(regs->tpc)) {
			insn = get_fault_insn(regs, insn);
			goto handle_kernel_fault;
		}
		down_read(&mm->mmap_sem);
	}

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;

	/* Pure DTLB misses do not tell us whether the fault causing
	 * load/store/atomic was a write or not, it only says that there
	 * was no match.  So in such a case we (carefully) read the
	 * instruction to try and figure this out.  It's an optimization
	 * so it's ok if we can't do this.
	 *
	 * Special hack, window spill/fill knows the exact fault type.
	 */
	if (((fault_code &
	      (FAULT_CODE_DTLB | FAULT_CODE_WRITE | FAULT_CODE_WINFIXUP)) == FAULT_CODE_DTLB) &&
	    (vma->vm_flags & VM_WRITE) != 0) {
		insn = get_fault_insn(regs, 0);
		if (!insn)
			goto continue_fault;
		if ((insn & 0xc0200000) == 0xc0200000 &&
		    (insn & 0x1780000) != 0x1680000) {
			/* Don't bother updating thread struct value,
			 * because update_mmu_cache only cares which tlb
			 * the access came from.
			 */
			fault_code |= FAULT_CODE_WRITE;
		}
	}
continue_fault:

	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (!(fault_code & FAULT_CODE_WRITE)) {
		/* Non-faulting loads shouldn't expand stack. */
		insn = get_fault_insn(regs, insn);
		if ((insn & 0xc0800000) == 0xc0800000) {
			unsigned char asi;

			if (insn & 0x2000)
				asi = (regs->tstate >> 24);
			else
				asi = (insn >> 5);
			if ((asi & 0xf2) == 0x82)
				goto bad_area;
		}
	}
	if (expand_stack(vma, address))
		goto bad_area;
	/*
	 * Ok, we have a good vm_area for this memory access, so
	 * we can handle it..
	 */
good_area:
	si_code = SEGV_ACCERR;

	/* If we took a ITLB miss on a non-executable page, catch
	 * that here.
	 */
	if ((fault_code & FAULT_CODE_ITLB) && !(vma->vm_flags & VM_EXEC)) {
		BUG_ON(address != regs->tpc);
		BUG_ON(regs->tstate & TSTATE_PRIV);
		goto bad_area;
	}

	if (fault_code & FAULT_CODE_WRITE) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;

		/* Spitfire has an icache which does not snoop
		 * processor stores.  Later processors do...
		 */
		if (tlb_type == spitfire &&
		    (vma->vm_flags & VM_EXEC) != 0 &&
		    vma->vm_file != NULL)
			set_thread_fault_code(fault_code |
					      FAULT_CODE_BLKCOMMIT);
	} else {
		/* Allow reads even for write-only mappings */
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
	}

	switch (handle_mm_fault(mm, vma, address, (fault_code & FAULT_CODE_WRITE))) {
	case VM_FAULT_MINOR:
		current->min_flt++;
		break;
	case VM_FAULT_MAJOR:
		current->maj_flt++;
		break;
	case VM_FAULT_SIGBUS:
		goto do_sigbus;
	case VM_FAULT_OOM:
		goto out_of_memory;
	default:
		BUG();
	}

	up_read(&mm->mmap_sem);
	return;

	/*
	 * Something tried to access memory that isn't in our memory map..
	 * Fix it, but check if it's kernel or user first..
	 */
bad_area:
	insn = get_fault_insn(regs, insn);
	up_read(&mm->mmap_sem);

handle_kernel_fault:
	do_kernel_fault(regs, si_code, fault_code, insn, address);
	return;

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	insn = get_fault_insn(regs, insn);
	up_read(&mm->mmap_sem);
	printk("VM: killing process %s\n", current->comm);
	if (!(regs->tstate & TSTATE_PRIV))
		do_exit(SIGKILL);
	goto handle_kernel_fault;

intr_or_no_mm:
	insn = get_fault_insn(regs, 0);
	goto handle_kernel_fault;

do_sigbus:
	insn = get_fault_insn(regs, insn);
	up_read(&mm->mmap_sem);

	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	do_fault_siginfo(BUS_ADRERR, SIGBUS, regs, insn, fault_code);

	/* Kernel mode? Handle exceptions or die */
	if (regs->tstate & TSTATE_PRIV)
		goto handle_kernel_fault;
}
