Rump kernel hypercalls for the Xen hypervisor [![Build Status](https://travis-ci.org/rumpkernel/rumprun-xen.png?branch=master)](https://travis-ci.org/rumpkernel/rumprun-xen)
=============================================

This repository contains supports for running rump kernels on top of the
Xen hypervisor and enables running off-the-shelf POSIX applications as
standalone Xen guests.

To build, install Xen headers, edit `domain_config` (top level in this
repo), and run:
````
./buildxen.sh
````

To run the demos, add `app-tools/` to the *end* of your `$PATH`. To run "Hello,
World!" no futher setup is required:

````
rumprun xen -di tests/hello/hello
````

To run a simple networking demo you will need Xen networking set up and run:

````
rumprun xen -di -n inet,static,10.10.10.10/24 tests/wopr/wopr
````

and try `telnet 10.10.10.10 4096`.

See [the wiki](http://wiki.rumpkernel.org/Repo:-rumprun-xen) for more
information and instructions.
