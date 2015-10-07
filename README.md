Rumprun [![Build Status](https://travis-ci.org/rumpkernel/rumprun.svg?branch=master)](https://travis-ci.org/rumpkernel/rumprun)
=======

This repository provides the Rumprun
[unikernel](https://en.wikipedia.org/wiki/Unikernel).  Rumprun runs on
not only on hypervisors such as KVM and Xen, but also on bare metal.
Rumprun can be used with or without a POSIX'y interface.  The former
allows existing, unmodified POSIX applications to run out-of-the-box,
while the latter allows building highly customized solutions with
minimal footprints.

See the [wiki](http://wiki.rumpkernel.org/Repo:-rumprun) for more
information and instructions.

Some of our tools will throw a warning about them
being experimental.  it does not mean that they
are not expected to produce a working result.  The wiki
[explains](http://wiki.rumpkernel.org/Repo%3A-rumprun#experimental-nature)
further.

You will find software packages for rumprun from the
[rumprun-packages repository](http://repo.rumpkernel.org/rumprun-packages).

hw
--

The hardware (``hw'') platform is meant for embedded systems
and the cloud.  It works on raw hardware, but also supports
_virtio_ drivers and KVM.  For a demonstration, see this [youtube
video](https://www.youtube.com/watch?v=EyeRplLMx4c) where the hw platform
is booted on a laptop and plays audio using the PCI hdaudio drivers.
The supported CPU architectures are x86_32, x86_64 and ARM.

Xen
---

The Xen platform is optimized for running on top of the Xen hypervisor
as a paravirtualized guest, and provides support for virtualization
functions not available on the _hw_ platform.  The Xen platform will
work both against the `xl` tools and the Amazon EC2 cloud.
The supported CPU architectures are x86_32 and x86_64.
