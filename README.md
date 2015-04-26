rumprun [![Build Status](https://travis-ci.org/rumpkernel/rumprun.svg?branch=master)](https://travis-ci.org/rumpkernel/rumprun)
=======

This repository provides the rumprun unikernel.  Rumprun enables
running existing POSIX-y software without an operating system on various
platforms, including bare metal and Xen.

See the [wiki](http://wiki.rumpkernel.org/Repo:-rumprun) for more
information and instructions.

baremetal
---------

The baremetal platform is meant for embedded systems and
the cloud.  It works on raw hardware, but also supports
_virtio_ drivers.  For a demostration, see this [youtube
video](https://www.youtube.com/watch?v=EyeRplLMx4c) where the baremetal
platform is booted on a laptop and plays audio using the PCI hdaudio
drivers.

Xen
---

The Xen platform is optimized for running on top of the Xen hypervisor,
and provides support for virtualization functions not available on the
_baremetal_ platform.
