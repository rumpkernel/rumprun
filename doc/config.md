# Rumprun unikernel configuration

This document specifies the JSON format used for configuring a rumprun unikernel
and the platform-dependent methods for passing configuration to a rumprun
unikernel.

*This is a work in progress, the described format and interfaces are not yet
stable! The document may not reflect the current implementation. Users are
advised to continue using the `rumprun` tool to launch rumprun unikernels until
further notice.*

Configuration interfaces and/or behaviours not documented here are considered
unofficial and experimental, and may be removed without warning.

## Configuration format

The configuration format is a valid JSON expression:

    { <JSON expression> }

Unless otherwise stated, all keys are optional.

### cmdline, rc: Program command line(s)

The `cmdline` key specifies the command line for unikernels containing a single
program:

    "cmdline": "arg0 arg1 arg2 ... argN"

_argN_: Corresponds to the `argv[N]` passed to the program.

Unikernels which contain more than one program (using multibake) MUST use
the `rc` key to specify command lines for each baked-in program:

    "rc": [
         {
             "bin" : "arg0",
             "argv" : [ "arg1", "arg2", ..., "argN" ],
             "runmode" : "& OR |" (optional)
         },
         ...
    ]

Each element of `rc` describes a single program, in the order in which they
are baked into the unikernel image.

_argN_: Corresponds to the `argv[N]` passed to the program.
_runmode_:
 * `&`: run this program in background
 * `|`: pipe output of current program to next defined program
 * (default): run this program in foreground and wait for it to successfully
   exit before running further programs

### env: Environment variables

    "env": "NAME=VALUE"
    ...

Sets the environment variable `NAME` to `VALUE`.

_FIXME_: Relies on specifying multiple `env` keys, which is not valid JSON.
Proposed specification as an array follows:

    "env": [
        "NAME=VALUE",
        ...
    ]

Sets the environment variable `NAME` to `VALUE`.

### hostname: Kernel hostname setting

    "hostname": "hostname"

_hostname_: The kernel hostname.

### net: Network interfaces

    "net": {
        "if": "if",
        "cloner": "cloner",
        "type": "type",
        "method": "method",
        ...
    }
    ...

_if_: The kernel name of the network interface. (eg. `vioif0`, `xenif0`)
_cloner_: Create the interface? (boolean, Xen only)
_type_, _method_: Network interface type and configuration method.

_TODO_: Document methods.

_FIXME_: Relies on specifying multiple `net` keys, which is not valid JSON.
Should be change to use an array instead.

### blk: Block devices and file systems

    "blk": {
        "source": "dev",
        "path": "/dev/ld0a",
        "fstype": "blk",
        "mountpoint": "/etc",
    }
    ...

_source_: `dev`, `vnd` or `etfs`
_path_: The path to the block device/etfs key. 
_fstype_: `blk` or `kernfs`
_mountpoint_: The filesystem mount point.

_TODO_: Incomplete. Document semantics of various sources, and differences
between Xen and hw.

_FIXME_: Relies on specifying multiple `blk` keys, which is not valid JSON.
Should be change to use an array instead.

## Passing configuration to the unikernel

### hw platform on x86

On x86 bare metal (this includes QEMU, KVM or other hypervisors using HVM)
rumprun uses the multiboot protocol.

Configuration may be passed either directly on the kernel command line, or
loaded as a multiboot module. If a multiboot module containing configuration is
found, it is used in preference to the kernel command line.

_TODO_: Provide examples. Also, can the `ROOTFSCFG=` hack go away now?

_TODO_: Make it explicit what a "multiboot module containing configuration"
means. Ignore modules which we don't understand (e.g. does not start with `{`).

### hw platform on ARM

TBD.

### xen platform on x86

On x86 paravirtualized Xen, the rumprun configuration must be written to the
domain-specific Xenstore key `rumprun/cfg` before the domU is started.

_TODO_: Examples. Do we officially want to support passing config on Xen via
cmdline (I think not).
