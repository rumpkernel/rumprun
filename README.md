rumprun [![Build Status](https://travis-ci.org/rumpkernel/rumprun.svg?branch=master)](https://travis-ci.org/rumpkernel/rumprun)
=======

This repository provides the rumprun unikernel.  Rumprun enables running
existing POSIX-y software without an operating system on various
platforms, including bare metal, KVM and Xen.  Both 32bit and 64bit
unikernels are supported on all platforms.

See the [wiki](http://wiki.rumpkernel.org/Repo:-rumprun) for more
information and instructions.

You will find software packages for rumprun from the
[rumprun-packages repository](http://repo.rumpkernel.org/rumprun-packages).

hw
--

The hardware (``hw'') platform is meant for embedded systems
and the cloud.  It works on raw hardware, but also supports
_virtio_ drivers and KVM.  For a demostration, see this [youtube
video](https://www.youtube.com/watch?v=EyeRplLMx4c) where the hw platform
is booted on a laptop and plays audio using the PCI hdaudio drivers.
The supported CPU architectures are x86 and ARM.

Xen
---

The Xen platform is optimized for running on top of the Xen hypervisor
as a paravirtualized guest, and provides support for virtualization
functions not available on the _hw_ platform.  The Xen platform will
work both against the `xl` tools and the Amazon EC2 cloud.
