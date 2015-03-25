Libraries hosted in this directory:

rumprun_core:
	"kernel" routines, these are self-contained and especially
	may *NOT* use libc interfaces

rumprun_base:
	"userspace" routines, implement some libc interfaces, etc.

rumprun_unwind:
	reachover library for NetBSD's stack unwind support (for C++)
