Rump kernels for "bare metal" [![Build Status](https://travis-ci.org/rumpkernel/rumprun-baremetal.svg?branch=master)](https://travis-ci.org/rumpkernel/rumprun-baremetal)
=======================================

This repository provides a simple "bare metal kernel" and a hypercall
implementation which enable running [rump kernels](http://rumpkernel.org/)
on bare metal.  By default, the produced image includes a TCP/IP stack,
a driver for the i82540 PCI NIC and of course system calls -- enough be
able to use TCP/IP via sockets.

See the [wiki page](http://wiki.rumpkernel.org/Repo:-rumprun-baremetal)
for information on building and running.

For a demonstration of rumprun-baremetal using the hdaudio device and
driver, see this
[youtube video](https://www.youtube.com/watch?v=EyeRplLMx4c).

Future directions
-----------------

This repo is meant as the base experiment for running rump
kernels on various virtualization platforms such as VMware,
VirtualBox and Hyper-V.  It will most likely be merged with
[rumprun-xen](http://repo.rumpkernel.org/rumprun-xen) at some point in
the future.
