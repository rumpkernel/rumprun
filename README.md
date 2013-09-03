Rump kernel hypercalls for the Xen hypervisor
=============================================

This repository contains code that implements the rump kernel hypercall
interfaces for the Xen hypervisor platform.  It enables running rump
kernels and application code as a single-image guest on top of Xen
without having to boot an entire OS.  The advantage of using rump
kernels is being able use unmodified kernel-quality drivers as part of
a single-image application, with the memory footprint being a fraction
of that of a full OS, yet still achieving the isolation provided by Xen.

The current status is working, with the block and NIC I/O devices having
been verified by using the Fast File System and TCP, respectively.
These drivers are also provided as demos, see instructions below.

For applications a POSIX-y interface is provided.  Some applications,
such as those using file systems or sockets interfaces, will more or less
work out-of-the-box.  Limitations include applications which do not fit
into a single-process no-VM model, such as applications using `fork()`,
`execve()` or file-backed `mmap()`.

See http://www.NetBSD.org/docs/rump/ for more information on rump kernels.


Using / Testing
---------------

To build and use, get the Xen source tree which matches your hypervisor
version.  Then, clone this repository into the `extras` subdirectory
of your Xen source tree and run the following command:

	./buildxen.sh

To run, use the standard Xen tools:

	xl create -c domain_config

Check out `domain_config` to change which tests/demos are run.
If you run the fs demo, copy the file system image
`test_clean.ffs` to `test.ffs` -- the image is also written, so this
avoids dirtying the version-controlled image.  If run the
networking demo, make sure your have a suitable Xen networking interface
(in addition to regular bridging, I had to use `ethtool -K if tx off` to
make connections from Dom0 work).  Also, check out what `rumpkern_demo.c`
actually does.


Implementation
--------------

The implementation runs on top of a slightly modified Xen Mini-OS, since
Mini-OS provided most of the functionality required for implementing the
rump kernel hypercalls, such as cooperative scheduling, etc.  See the
`rumphyper*` files to see the hypercall implementations relevant for
running rump kernels.
