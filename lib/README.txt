Libraries hosted in this directory:

bmk_core:
	"kernel" routines, these are self-contained and especially
	may *NOT* use libc interfaces

bmk_rumpuser:
	common bits for the rump kernel hypercall implementation,
	runs on top of bmk.

rumprun_base:
	"userspace" routines, implement some libc interfaces, etc.

rumprun_unwind:
	reachover library for NetBSD's stack unwind support (for C++)
