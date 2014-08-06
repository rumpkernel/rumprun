Rump kernel hypervisor for "bare metal"
=======================================

This repository contains a simple, and highly experimental, "bare
metal kernel" and a hypercall implementation which enable running [rump
kernels](http://rumpkernel.org/) directly on bare metal.  By default, the
produced image includes a TCP/IP stack, a driver for the i82540 PCI NIC
and of course system calls -- enough be able to use TCP/IP via sockets.

Edit the Makefile and compile with:

```
make
```

Run for example with:

```
qemu-system-i386 -s -net nic -net user -kernel rk.bin
```

(Top tip: `-s` enables the qemu remote debugging service.  You can
connect to it from `gdb` with `target remote:1234`)

Future directions
-----------------

This repo is meant as the base experiment for running rump kernels on
various virtualization platforms such as VMware, VirtualBox and Hyper-V.
When we add support for easy running of POSIX applications, most likely
the contents of this repository will migrate to a `rumprun-X` repository
(cf. http://repo.rumpkernel.org/).
