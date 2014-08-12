Rump kernel hypervisor for "bare metal"
=======================================

This repository contains a simple, and highly experimental, "bare
metal kernel" and a hypercall implementation which enable running [rump
kernels](http://rumpkernel.org/) directly on bare metal.  By default, the
produced image includes a TCP/IP stack, a driver for the i82540 PCI NIC
and of course system calls -- enough be able to use TCP/IP via sockets.

See the [wiki page](http://wiki.rumpkernel.org/Repo:-rumpuser-baremetal)
for information on building and running.

Future directions
-----------------

This repo is meant as the base experiment for running rump kernels on
various virtualization platforms such as VMware, VirtualBox and Hyper-V.
When we add support for easy running of POSIX applications, most likely
the contents of this repository will migrate to a `rumprun-X` repository
(cf. http://repo.rumpkernel.org/).
