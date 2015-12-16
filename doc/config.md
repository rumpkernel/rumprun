# Rumprun unikernel configuration

This document specifies the format used for configuring a rumprun unikernel and
the platform-dependent methods for passing configuration to a rumprun unikernel.

The intent of this specification is to define the _minimal_ "lowest common
denominator" for configuring a rumprun unikernel. It is expected that special
case configuration will be performed by the programs baked into the unikernel on
a case-by-case basis rather than specified here.

*This is a work in progress, the described format and interfaces are not yet
stable! The document may not reflect the current implementation. Users are
advised to continue using the `rumprun` tool to launch rumprun unikernels until
further notice.*

Configuration interfaces and/or behaviours not documented here are considered
unofficial and experimental, and may be removed without warning.

A rumprun unikernel does not _require_ any configuration to be passed for it to
boot. However, if the `rc` key is not specified, only the first multibaked
program in the unikernel will be invoked.

# Configuration format

The configuration format is defined using JSON:

    { <JSON expression> }

When configuration is passed directly on the kernel command line (see
platform-specific methods below), everything up to the first `{` character is
not considered to be part of the configuration.

## rc: Program invocation

    "rc": [
         {
             "bin" : <string>,
             "argv" : [ <string>, ... ],
             "runmode" : "& OR |" (optional)
         },
         ...
    ]

Each element of `rc` describes a single program, in the order in which they
are baked into the unikernel image.

* _bin_: Passed to the corresponding program as `argv[0]`.
* _argv[]_: Passed to the corresponding program as `argv[1..N]`.
* _runmode_: Defines how the corresponding program will be invoked. _Optional_
  * `&`: run program in background.
  * `|`: pipe output of program to next defined program.
  * _default_: run program in foreground and wait for it to exit successfully
    before running any further programs.

## env: Environment variables

    "env": {
        <key>: <value>,
        ...
    ]

* _env_: Each _key_/_value_ pair of strings sets the environment variable
  `<key>` to `<value>`.

## hostname: Kernel hostname

    "hostname": <string>

* _hostname_: Sets the hostname returned by the `gethostname()` call.

## net: Network interfaces

Each `net` key defines a network interface to configure:

    "net": {
        "if": <string>,
        "cloner": <boolean>,
        "type": <string>,
        <type-specific keys>
    }
    ...

* _if_: The name of the network interface, as seen by the rump kernel. (eg.
  `vioif0`, `xenif0`)
* _cloner_: If true, the rump kernel interface is created at boot time. Required
  for Xen netback interfaces.
* _type_: Network interface type. Supported values are `inet` or `inet6`.

_FIXME_: Relies on specifying multiple `net` keys, which is not valid JSON.
Should be change to use an array instead.

### inet: IPv4 configuration

A `type` of `inet` indicates that this key defines an interface to be configured
using IPv4. The `method` key must be set to one of the following:

* `dhcp`: Configure the interface using DHCPv4.
* `static`: Configure the interface statically. The following additional keys
  must be present:
  * `addr`: IPv4 interface address.
  * `mask`: IPv4 interface netmask in CIDR format.
  * `gw`: IPv4 address of default gateway. _Optional._

### inet6: IPv6 configuration

A `type` of `inet6` indicates that this key defines an interface to be
configured using IPv6. The `method` key must be set to one of the following:

* `auto`: Configure the interface using IPv6 stateless autoconfiguration.
* `static`: Configure the interface statically. The following additional keys
  must be present:
  * `addr`: IPv6 interface address.
  * `mask`: IPv6 interface netmask in CIDR format.
  * `gw`: IPv6 address of default gateway. _Optional._

## blk: Block devices and filesystems

Each `blk` key defines a block device and filesystem to mount:

    "blk": {
        "source": <string>,
        <source-specific keys>
        "path": "/dev/ld0a",
        "fstype": "blk",
        "mountpoint": "/etc",
    }
    ...

* _source_: One of `dev`, `vnd` or `etfs`.

_FIXME_: Relies on specifying multiple `blk` keys, which is not valid JSON.
Should be change to use an array instead.

_FIXME_: Unclear from the code when a `blk` key can be used to configure, but
not mount, a block device.

### dev: Mount filesystem backed by block device

A `source` of `dev` indicates that this key defines a filesystem backed by a
rump kernel block device. Block devices are usually used by the rumprun
unikernel to access directly-attached storage on bare metal, or `virtio` devices
on QEMU/KVM.

The following additional keys are required:

* _mountpoint_: The mountpoint for the filesystem.
* _fstype_: If set to `kern`, a `kernfs` will be mounted on `mountpoint` and all
  other keys will be ignored. If set to `blk`, rumprun will attempt to mount a
  filesystem of type `ffs`, `ext2fs` or `cd9660` from the local block device
  specified by `path`.
* _path_: The pathname of the block device to mount, as seen by the rump kernel.

_TODO_: Specify example _paths_ for block devices (`/dev/ld0X`, `/dev/sd0X`).

### etfs: Mount filesystem backed by rump_etfs "host" device

A `source` of `etfs` indicates that this key defines a filesystem backed by a
`rump_etfs(3)` device. ETFS devices are usually used by the rumprun unikernel to
access storage on the Xen platform.

The following additional keys are required:

* _mountpoint_: The mountpoint for the filesystem.
* _fstype_: Must be set to `blk`. Rumprun will attempt to mount a
  filesystem of type `ffs`, `ext2fs` or `cd9660` from the etfs device specified
  by `path`.
* _path_: The platform-specific `key` passed to `rump_pub_etfs_register()`.

_TODO_: Specify example _paths_ for block devices used on Xen.

### vnd: Mount filesystem backed by a vnode disk device

_TODO_: Complete this section.

# Passing configuration to the unikernel

## hw platform on x86

On x86 bare metal (this includes QEMU, KVM or other hypervisors using HVM)
rumprun uses the multiboot protocol.

Configuration may be passed either directly on the kernel command line, or
loaded as a multiboot module. If a multiboot module containing configuration is
found, it is used in preference to the kernel command line.

_TODO_: Provide examples. Also, can the `ROOTFSCFG=` hack go away now that we
can load configuration using multiboot.

_TODO_: Make it explicit what a "multiboot module containing configuration"
means. Ignore modules which we don't understand (e.g. does not start with `{`).

## xen platform on x86

On x86 paravirtualized Xen, the rumprun configuration must be written to the
domain-specific Xenstore key `rumprun/cfg` before the domU is started.

_TODO_: Examples. Do we officially want to support passing config on Xen via
cmdline (I think not).

_TODO_: Explain what "domain-specific Xenstore key" means.

## hw platform on ARM

TBD.
