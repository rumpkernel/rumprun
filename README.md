Rump kernel hypercalls for the Xen hypervisor
=============================================

This repository contains code that implements the rump kernel hypercall
interface for the Xen hypervisor platform.  It enables running rump
kernels and application code as a single-image guest on top of Xen
without having to boot an entire OS.  The advantage is using rump
kernels is being able run hundreds of thousands of lines of unmodified
kernel-quality drivers as part of the single-image application.

The current status is working, with the block and NIC I/O devices having
been verified by using the Fast File System and TCP, respectively.
The immediate TODO is cleaning up the build procedure.

See http://www.NetBSD.org/docs/rump for more information on rump kernels.

Using / Testing
---------------

To run the demos, you need a Xen Dom0 (surprise).  Additionally, you need
to build rump kernel components for your target system.  Even if your
system supplies rump kernel components as binaries, you need a fairly
recent build.  The easiest way to ensure this is to use
[buildrump.sh](https://github.com/anttikantee/buildrump.sh), which will
get the necessary things done with a single command.

To build and use, get the Xen source tree which matches your hypervisor
version.  Then, clone this repository into the `extras` subdirectory
of your Xen source tree and follow/copypaste the commands below.

First, build rump kernel components for your machine architecture:

	git submodule update --init --recursive
	./buildrump.sh/buildrump.sh -s rumpsrc -T rumptools -o rumpobj -V NOPIC=1

Next, build networking support (this step will be integrated into the
build at a later date, but required as a separate step for now).

	( cd rumpxenif ; ../rumptools/rumpmake dependall && ../rumptools/rumpmake install )

Now you can build your domU image:

	make

If you want to run the fs demo, copy the file system image
`test_clean.ffs` to `test.ffs` -- the image is also written, so this
avoids dirtying the version-controlled image.  If you want to run the
networking demo, make sure your have a suitable Xen networking interface
(in addition to regular bridging, I had to use `ethtool -K if tx off` to
make connections from Dom0 work).  Also, check out what `rumpkern_demo.c`
actually does.

To run, use the standard Xen tools:

	xl create -c domain_config


Implementation
--------------

The implementation runs on top of a slightly modified Xen Mini-OS, since
Mini-OS provided most of the functionality required for implementing the
rump kernel hypercalls, such as cooperative scheduling, etc.  See the
`rumphyper*` files to see the hypercall implementations relevant for
running rump kernels.
