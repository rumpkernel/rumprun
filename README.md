Rump kernel hypercalls for the Xen hypervisor [![Build Status](https://travis-ci.org/rumpkernel/rumpuser-xen.png?branch=master)](https://travis-ci.org/rumpkernel/rumpuser-xen)
=============================================

This repository contains supports for running rump kernels on top of the
Xen hypervisor and enables running off-the-shelf POSIX applications as
standalone Xen guests.

To install, install xen headers and do
````
./buildxen.sh
xl create -c domain_config
````

See [the wiki](http://wiki.rumpkernel.org/Repo:-rumpuser-xen) for more
information and instructions.
