#include <mini-os/x86/os.h>
#include <mini-os/x86/limits.h>
#include <xen/features.h>

.section __xen_guest
	.ascii	"GUEST_OS=rumprun"
	.ascii	",XEN_VER=xen-3.0"
	.ascii	",VIRT_BASE=0x0" /* &_text from minios_x86_64.lds */
	.ascii	",ELF_PADDR_OFFSET=0x0"
	.ascii	",HYPERCALL_PAGE=0x2"
	.ascii	",LOADER=generic"
	.byte	0
.text

#define ENTRY(name) \
        .globl _minios_entry_##name; \
        _minios_entry_##name:

.globl _start, _minios_shared_info, _minios_hypercall_page


_start:
        cld
        movq stack_start(%rip),%rsp
        andq $(~(__STACK_SIZE-1)), %rsp
        pushq $0
        pushq $0
        xorq %rbp,%rbp
        movq %rsi,%rdi
        call _minios_start_kernel

stack_start:
        .quad _minios_stack+(2*__STACK_SIZE)

        /* Unpleasant -- the PTE that maps this page is actually overwritten */
        /* to map the real shared-info page! :-)                             */
        .org 0x1000
_minios_shared_info:
        .org 0x2000

_minios_hypercall_page:
        .org 0x3000


/* Offsets into shared_info_t. */                
#define evtchn_upcall_pending		/* 0 */
#define evtchn_upcall_mask		1

NMI_MASK = 0x80000000

#define RDI 112
#define ORIG_RAX 120       /* + error_code */ 
#define EFLAGS 144

.macro RESTORE_ALL
	movq (%rsp),%r11
	movq 1*8(%rsp),%r10
	movq 2*8(%rsp),%r9
	movq 3*8(%rsp),%r8
	movq 4*8(%rsp),%rax
	movq 5*8(%rsp),%rcx
	movq 6*8(%rsp),%rdx
	movq 7*8(%rsp),%rsi
	movq 8*8(%rsp),%rdi
	addq $9*8+8,%rsp
.endm	


.macro HYPERVISOR_IRET flag
	testl $NMI_MASK,2*8(%rsp)
	jnz   2f

	testb $1,(_minios_xen_features+XENFEAT_supervisor_mode_kernel)
	jnz   1f

	/* Direct iret to kernel space. Correct CS and SS. */
	orb   $3,1*8(%rsp)
	orb   $3,4*8(%rsp)
1:	iretq

2:	/* Slow iret via hypervisor. */
	andl  $~NMI_MASK, 16(%rsp)
	pushq $\flag
	jmp  _minios_hypercall_page + (__HYPERVISOR_iret * 32)
.endm

/*
 * Common code to all exception entry points. This expects an error
 * code/orig_rax on the stack and the exception handler in %rax.	
 */ 		  				
error_common:
	/* rdi slot contains rax, oldrax contains error code */
	cld	
	subq  $14*8,%rsp
	movq %rsi,13*8(%rsp)
	movq 14*8(%rsp),%rsi	/* load rax from rdi slot */
	movq %rdx,12*8(%rsp)
	movq %rcx,11*8(%rsp)
	movq %rsi,10*8(%rsp)	/* store rax */ 
	movq %r8, 9*8(%rsp)
	movq %r9, 8*8(%rsp)
	movq %r10,7*8(%rsp)
	movq %r11,6*8(%rsp)
	movq %rbx,5*8(%rsp) 
	movq %rbp,4*8(%rsp) 
	movq %r12,3*8(%rsp) 
	movq %r13,2*8(%rsp) 
	movq %r14,1*8(%rsp) 
	movq %r15,(%rsp) 

error_call_handler:
	movq %rdi, RDI(%rsp)            
	movq %rsp,%rdi
	movq ORIG_RAX(%rsp),%rsi	# get error code 
	movq $-1,ORIG_RAX(%rsp)
	call *%rax
	jmp error_exit

.macro errorentry sym has_error_code:req
        movq (%rsp),%rcx
        movq 8(%rsp),%r11
        addq $0x10,%rsp /* skip rcx and r11 */
        .if !\has_error_code
	pushq $0	/* push error code/oldrax */
        .endif
	pushq %rax	/* push real oldrax to the rdi slot */ 
	leaq  \sym(%rip),%rax
	jmp error_common
.endm	

#define XEN_GET_VCPU_INFO(reg)	movq HYPERVISOR_shared_info,reg
#define XEN_PUT_VCPU_INFO(reg)
#define XEN_PUT_VCPU_INFO_fixup
#define XEN_LOCKED_BLOCK_EVENTS(reg)	movb $1,evtchn_upcall_mask(reg)
#define XEN_LOCKED_UNBLOCK_EVENTS(reg)	movb $0,evtchn_upcall_mask(reg)
#define XEN_TEST_PENDING(reg)	testb $0xFF,evtchn_upcall_pending(reg)

#define XEN_BLOCK_EVENTS(reg)	XEN_GET_VCPU_INFO(reg)			; \
                    			XEN_LOCKED_BLOCK_EVENTS(reg)	; \
    				            XEN_PUT_VCPU_INFO(reg)

#define XEN_UNBLOCK_EVENTS(reg)	XEN_GET_VCPU_INFO(reg)			; \
                				XEN_LOCKED_UNBLOCK_EVENTS(reg)	; \
    			            	XEN_PUT_VCPU_INFO(reg)



ENTRY(hypervisor_callback)
    errorentry hypervisor_callback2 0

hypervisor_callback2:
        movq %rdi, %rsp 
11:     movq %gs:8,%rax
        incl %gs:0
        cmovzq %rax,%rsp
        pushq %rdi
        call _minios_do_hypervisor_callback 
        popq %rsp
        decl %gs:0
        jmp error_exit

restore_all_enable_events:  
	XEN_UNBLOCK_EVENTS(%rsi)        # %rsi is already set up...

scrit:	/**** START OF CRITICAL REGION ****/
	XEN_TEST_PENDING(%rsi)
	jnz  14f			# process more events if necessary...
	XEN_PUT_VCPU_INFO(%rsi)
        RESTORE_ALL
        HYPERVISOR_IRET 0
        
14:	XEN_LOCKED_BLOCK_EVENTS(%rsi)
	XEN_PUT_VCPU_INFO(%rsi)
	subq $6*8,%rsp
	movq %rbx,5*8(%rsp) 
	movq %rbp,4*8(%rsp) 
	movq %r12,3*8(%rsp) 
	movq %r13,2*8(%rsp) 
	movq %r14,1*8(%rsp) 
	movq %r15,(%rsp) 
        movq %rsp,%rdi                  # set the argument again
	jmp  11b
ecrit:  /**** END OF CRITICAL REGION ****/


retint_kernel:
retint_restore_args:
	movl EFLAGS-6*8(%rsp), %eax
	shr $9, %eax			# EAX[0] == IRET_EFLAGS.IF
	XEN_GET_VCPU_INFO(%rsi)
	andb evtchn_upcall_mask(%rsi),%al
	andb $1,%al			# EAX[0] == IRET_EFLAGS.IF & event_mask
	jnz restore_all_enable_events	#        != 0 => enable event delivery
	XEN_PUT_VCPU_INFO(%rsi)
		
	RESTORE_ALL
	HYPERVISOR_IRET 0


error_exit:		
	movq (%rsp),%r15
	movq 1*8(%rsp),%r14
	movq 2*8(%rsp),%r13
	movq 3*8(%rsp),%r12
	movq 4*8(%rsp),%rbp
	movq 5*8(%rsp),%rbx
	addq $6*8,%rsp
	XEN_BLOCK_EVENTS(%rsi)		
	jmp retint_kernel



ENTRY(failsafe_callback)
        popq  %rcx
        popq  %r11
        iretq


ENTRY(coprocessor_error)
        errorentry do_coprocessor_error 0


ENTRY(simd_coprocessor_error)
        errorentry do_simd_coprocessor_error 0


ENTRY(device_not_available)
        errorentry do_device_not_available 0


ENTRY(debug)
        errorentry do_debug 0


ENTRY(int3)
        errorentry do_int3 0

ENTRY(overflow)
        errorentry do_overflow 0


ENTRY(bounds)
        errorentry do_bounds 0
    
    
ENTRY(invalid_op)
        errorentry do_invalid_op 0


ENTRY(coprocessor_segment_overrun)
        errorentry do_coprocessor_segment_overrun 0


ENTRY(invalid_TSS)
        errorentry do_invalid_TSS 1


ENTRY(segment_not_present)
        errorentry do_segment_not_present 1


/* runs on exception stack */
ENTRY(stack_segment)
        errorentry do_stack_segment 1
                    

ENTRY(general_protection)
        errorentry do_general_protection 1


ENTRY(alignment_check)
        errorentry do_alignment_check 1


ENTRY(divide_error)
        errorentry do_divide_error 0


ENTRY(spurious_interrupt_bug)
        errorentry do_spurious_interrupt_bug 0
            

ENTRY(page_fault)
        errorentry do_page_fault 1
