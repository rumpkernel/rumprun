
#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/mm.h>
#include <mini-os/lib.h>

#include <mini-os/machine/traps.h>

#include <bmk-core/string.h>

/*
 * These are assembler stubs in entry.S.
 * They are the actual entry points for virtual exceptions.
 */
void _minios_entry_divide_error(void);
void _minios_entry_debug(void);
void _minios_entry_int3(void);
void _minios_entry_overflow(void);
void _minios_entry_bounds(void);
void _minios_entry_invalid_op(void);
void _minios_entry_device_not_available(void);
void _minios_entry_coprocessor_segment_overrun(void);
void _minios_entry_invalid_TSS(void);
void _minios_entry_segment_not_present(void);
void _minios_entry_stack_segment(void);
void _minios_entry_general_protection(void);
void _minios_entry_page_fault(void);
void _minios_entry_coprocessor_error(void);
void _minios_entry_simd_coprocessor_error(void);
void _minios_entry_alignment_check(void);
void _minios_entry_spurious_interrupt_bug(void);
void _minios_entry_machine_check(void);


void dump_regs(struct pt_regs *regs)
{
    minios_printk("Thread: %s\n", bmk_sched_threadname(bmk_current));
#ifdef __i386__    
    minios_printk("EIP: %x, EFLAGS %x.\n", regs->eip, regs->eflags);
    minios_printk("EBX: %08x ECX: %08x EDX: %08x\n",
	   regs->ebx, regs->ecx, regs->edx);
    minios_printk("ESI: %08x EDI: %08x EBP: %08x EAX: %08x\n",
	   regs->esi, regs->edi, regs->ebp, regs->eax);
    minios_printk("DS: %04x ES: %04x orig_eax: %08x, eip: %08x\n",
	   regs->xds, regs->xes, regs->orig_eax, regs->eip);
    minios_printk("CS: %04x EFLAGS: %08x esp: %08x ss: %04x\n",
	   regs->xcs, regs->eflags, regs->esp, regs->xss);
#else
    minios_printk("RIP: %04lx:[<%016lx>] ", regs->cs & 0xffff, regs->rip);
    minios_printk("\nRSP: %04lx:%016lx  EFLAGS: %08lx\n", 
           regs->ss, regs->rsp, regs->eflags);
    minios_printk("RAX: %016lx RBX: %016lx RCX: %016lx\n",
           regs->rax, regs->rbx, regs->rcx);
    minios_printk("RDX: %016lx RSI: %016lx RDI: %016lx\n",
           regs->rdx, regs->rsi, regs->rdi); 
    minios_printk("RBP: %016lx R08: %016lx R09: %016lx\n",
           regs->rbp, regs->r8, regs->r9); 
    minios_printk("R10: %016lx R11: %016lx R12: %016lx\n",
           regs->r10, regs->r11, regs->r12); 
    minios_printk("R13: %016lx R14: %016lx R15: %016lx\n",
           regs->r13, regs->r14, regs->r15); 
#endif
}

static void do_trap(int trapnr, char *str, struct pt_regs * regs, unsigned long error_code)
{
    minios_printk("FATAL:  Unhandled Trap %d (%s), error code=0x%lx\n", trapnr, str, error_code);
    minios_printk("Regs address %p\n", regs);
    dump_regs(regs);
    minios_do_exit();
}

#define DO_ERROR(trapnr, str, name) \
void do_##name(struct pt_regs * regs, unsigned long error_code) \
{ \
	do_trap(trapnr, str, regs, error_code); \
}

#define DO_ERROR_INFO(trapnr, str, name, sicode, siaddr) \
void do_##name(struct pt_regs * regs, unsigned long error_code) \
{ \
	do_trap(trapnr, str, regs, error_code); \
}

DO_ERROR_INFO( 0, "divide error", divide_error, FPE_INTDIV, regs->eip)
DO_ERROR( 3, "int3", int3)
DO_ERROR( 4, "overflow", overflow)
DO_ERROR( 5, "bounds", bounds)
DO_ERROR_INFO( 6, "invalid operand", invalid_op, ILL_ILLOPN, regs->eip)
DO_ERROR( 7, "device not available", device_not_available)
DO_ERROR( 9, "coprocessor segment overrun", coprocessor_segment_overrun)
DO_ERROR(10, "invalid TSS", invalid_TSS)
DO_ERROR(11, "segment not present", segment_not_present)
DO_ERROR(12, "stack segment", stack_segment)
DO_ERROR_INFO(17, "alignment check", alignment_check, BUS_ADRALN, 0)
DO_ERROR(18, "machine check", machine_check)

void page_walk(unsigned long virt_address)
{
        pgentry_t *tab = (pgentry_t *)start_info.pt_base, page;
        unsigned long addr = virt_address;
        minios_printk("Pagetable walk from virt %lx, base %lx:\n", virt_address, start_info.pt_base);
    
#if defined(__x86_64__)
        page = tab[l4_table_offset(addr)];
        tab = pte_to_virt(page);
        minios_printk(" L4 = %"PRIpte" (%p)  [offset = %lx]\n", page, tab, l4_table_offset(addr));
#endif
        page = tab[l3_table_offset(addr)];
        tab = pte_to_virt(page);
        minios_printk("  L3 = %"PRIpte" (%p)  [offset = %lx]\n", page, tab, l3_table_offset(addr));
        page = tab[l2_table_offset(addr)];
        tab = pte_to_virt(page);
        minios_printk("   L2 = %"PRIpte" (%p)  [offset = %lx]\n", page, tab, l2_table_offset(addr));
        
        page = tab[l1_table_offset(addr)];
        minios_printk("    L1 = %"PRIpte" [offset = %lx]\n", page, l1_table_offset(addr));

}

static int handle_cow(unsigned long addr) {
        pgentry_t *tab = (pgentry_t *)start_info.pt_base, page;
	unsigned long new_page;
	int rc;

#if defined(__x86_64__)
        page = tab[l4_table_offset(addr)];
	if (!(page & _PAGE_PRESENT))
	    return 0;
        tab = pte_to_virt(page);
#endif
        page = tab[l3_table_offset(addr)];
	if (!(page & _PAGE_PRESENT))
	    return 0;
        tab = pte_to_virt(page);

        page = tab[l2_table_offset(addr)];
	if (!(page & _PAGE_PRESENT))
	    return 0;
        tab = pte_to_virt(page);
        
        page = tab[l1_table_offset(addr)];
	if (!(page & _PAGE_PRESENT))
	    return 0;
	/* Only support CoW for the zero page.  */
	if (PHYS_PFN(page) != _minios_mfn_zero)
	    return 0;

	new_page = minios_alloc_pages(0);
	bmk_memset((void*) new_page, 0, PAGE_SIZE);

	rc = HYPERVISOR_update_va_mapping(addr & PAGE_MASK, __pte(virt_to_mach(new_page) | L1_PROT), UVMF_INVLPG);
	if (!rc)
		return 1;

	minios_printk("Map zero page to %lx failed: %d.\n", addr, rc);
	return 0;
}

static void do_stack_walk(unsigned long frame_base)
{
    unsigned long *frame = (void*) frame_base;
    minios_printk("base is %#lx ", frame_base);
    minios_printk("caller is %#lx\n", frame[1]);
    if (frame[0])
	do_stack_walk(frame[0]);
}

void stack_walk(void)
{
    unsigned long bp;
#ifdef __x86_64__
    asm("movq %%rbp, %0":"=r"(bp));
#else
    asm("movl %%ebp, %0":"=r"(bp));
#endif
    do_stack_walk(bp);
}

static void dump_mem(unsigned long addr)
{
    unsigned long i;
    if (addr < PAGE_SIZE)
	return;

    for (i = ((addr)-16 ) & ~15; i < (((addr)+48 ) & ~15); i++)
    {
	if (!(i%16))
	    minios_printk("\n%lx:", i);
	minios_printk(" %02x", *(unsigned char *)i);
    }
    minios_printk("\n");
}
#define read_cr2() \
        (HYPERVISOR_shared_info->vcpu_info[smp_processor_id()].arch.cr2)

static int handling_pg_fault = 0;

void do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
    unsigned long addr = read_cr2();
    struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };

    if ((error_code & TRAP_PF_WRITE) && handle_cow(addr))
	return;

    /* If we are already handling a page fault, and got another one
       that means we faulted in pagetable walk. Continuing here would cause
       a recursive fault */       
    if(handling_pg_fault == 1) 
    {
        minios_printk("Page fault in pagetable walk (access to invalid memory?).\n"); 
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
    handling_pg_fault++;
    barrier();

#if defined(__x86_64__)
    minios_printk("Page fault at linear address %p, rip %p, regs %p, sp %p, our_sp %p, code %lx\n",
           addr, regs->rip, regs, regs->rsp, &addr, error_code);
#else
    minios_printk("Page fault at linear address %p, eip %p, regs %p, sp %p, our_sp %p, code %lx\n",
           addr, regs->eip, regs, regs->esp, &addr, error_code);
#endif

    dump_regs(regs);
#if defined(__x86_64__)
    do_stack_walk(regs->rbp);
    dump_mem(regs->rsp);
    dump_mem(regs->rbp);
    dump_mem(regs->rip);
#else
    do_stack_walk(regs->ebp);
    dump_mem(regs->esp);
    dump_mem(regs->ebp);
    dump_mem(regs->eip);
#endif
    page_walk(addr);
    HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    /* We should never get here ... but still */
    handling_pg_fault--;
}

void do_general_protection(struct pt_regs *regs, long error_code)
{
    struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
#ifdef __i386__
    minios_printk("GPF eip: %p, error_code=%lx\n", regs->eip, error_code);
#else    
    minios_printk("GPF rip: %p, error_code=%lx\n", regs->rip, error_code);
#endif
    dump_regs(regs);
#if defined(__x86_64__)
    do_stack_walk(regs->rbp);
    dump_mem(regs->rsp);
    dump_mem(regs->rbp);
    dump_mem(regs->rip);
#else
    do_stack_walk(regs->ebp);
    dump_mem(regs->esp);
    dump_mem(regs->ebp);
    dump_mem(regs->eip);
#endif
    HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
}


void do_debug(struct pt_regs * regs)
{
    minios_printk("Debug exception\n");
#define TF_MASK 0x100
    regs->eflags &= ~TF_MASK;
    dump_regs(regs);
    minios_do_exit();
}

void do_coprocessor_error(struct pt_regs * regs)
{
    minios_printk("Copro error\n");
    dump_regs(regs);
    minios_do_exit();
}

void simd_math_error(void *eip)
{
    minios_printk("SIMD error\n");
}

void do_simd_coprocessor_error(struct pt_regs * regs)
{
    minios_printk("SIMD copro error\n");
}

void do_spurious_interrupt_bug(struct pt_regs * regs)
{
}

/*
 * Submit a virtual IDT to teh hypervisor. This consists of tuples
 * (interrupt vector, privilege ring, CS:EIP of handler).
 * The 'privilege ring' field specifies the least-privileged ring that
 * can trap to that vector using a software-interrupt instruction (INT).
 */
static trap_info_t trap_table[] = {
    {  0, 0, __KERNEL_CS, (unsigned long)_minios_entry_divide_error                },
    {  1, 0, __KERNEL_CS, (unsigned long)_minios_entry_debug                       },
    {  3, 3, __KERNEL_CS, (unsigned long)_minios_entry_int3                        },
    {  4, 3, __KERNEL_CS, (unsigned long)_minios_entry_overflow                    },
    {  5, 3, __KERNEL_CS, (unsigned long)_minios_entry_bounds                      },
    {  6, 0, __KERNEL_CS, (unsigned long)_minios_entry_invalid_op                  },
    {  7, 0, __KERNEL_CS, (unsigned long)_minios_entry_device_not_available        },
    {  9, 0, __KERNEL_CS, (unsigned long)_minios_entry_coprocessor_segment_overrun },
    { 10, 0, __KERNEL_CS, (unsigned long)_minios_entry_invalid_TSS                 },
    { 11, 0, __KERNEL_CS, (unsigned long)_minios_entry_segment_not_present         },
    { 12, 0, __KERNEL_CS, (unsigned long)_minios_entry_stack_segment               },
    { 13, 0, __KERNEL_CS, (unsigned long)_minios_entry_general_protection          },
    { 14, 0, __KERNEL_CS, (unsigned long)_minios_entry_page_fault                  },
    { 15, 0, __KERNEL_CS, (unsigned long)_minios_entry_spurious_interrupt_bug      },
    { 16, 0, __KERNEL_CS, (unsigned long)_minios_entry_coprocessor_error           },
    { 17, 0, __KERNEL_CS, (unsigned long)_minios_entry_alignment_check             },
    { 19, 0, __KERNEL_CS, (unsigned long)_minios_entry_simd_coprocessor_error      },
    {  0, 0,           0, 0                           }
};
    


void trap_init(void)
{
    HYPERVISOR_set_trap_table(trap_table);    
}

void trap_fini(void)
{
    HYPERVISOR_set_trap_table(NULL);
}
