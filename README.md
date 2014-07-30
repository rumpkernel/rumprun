Rump kernel hypercalls for the Xen hypervisor [![Build Status](https://travis-ci.org/rumpkernel/rumprun-xen.png?branch=master)](https://travis-ci.org/rumpkernel/rumprun-xen)
=============================================

This repository contains supports for running rump kernels on top of the
Xen hypervisor and enables running off-the-shelf POSIX applications as
standalone Xen guests.

To use, install Xen headers, edit `domain_config` (top level in this
repo), and run:
````
./buildxen.sh
xl create -c domain_config
````

See [the wiki](http://wiki.rumpkernel.org/Repo:-rumprun-xen) for more
information and instructions.
