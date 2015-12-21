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

## net: Network configuration

    "net": {
        "interfaces": {
            "<name>": {
                "create": <boolean>,
                "addrs": [
                    <address>,
                    ...
                ]
            },
            ...
        },
        "gateways": [
            <gateway>,
            ...
        ]
    }

* _interfaces_: Each key configures a single network interface:
  * _name_: The name of the network interface, as seen by the rump kernel. (eg.
    `vioif0`, `xenif0`)
  * _create_: If true, the rump kernel interface is created at boot time.
    Required for Xen netback interfaces, otherwise _optional_.
  * _address_: Configures a network address to be assigned to the interface.
    Refer to protocol families below for supported `<address>` syntax.
    _Optional_
* _gateways_: Configures default gateways. Refer to protocol families below for
  supported `<gateway>` syntax. At most one `<gateway>` may be configured for
  each supported network protocol. _Optional_

### IPv4 configuration

Interface addresses to be configured using IPv4 are specified as follows:

    {
        "type": "inet",
        "method": <string>,
        <method-specific configuration>
    }

The `method` key must be set to one of the following:

* `dhcp`: Configure the interface using DHCPv4.
* `static`: Configure the interface statically. The following additional keys
  must be present:
  * `addr`: IPv4 interface address, in CIDR `address/mask` format.

Gateways to be configured using IPv4 are specified as follows:

    {
        "type": "inet",
        "addr": <string>
    }

* _addr_: The IPv4 address of the default gateway.

### IPv6 configuration

Interface addresses to be configured using IPv6 are specified as follows:

    {
        "type": "inet6",
        "method": <string>,
        <method-specific configuration>
    }

The `method` key must be set to one of the following:

* `auto`: Configure the interface using IPv6 stateless autoconfiguration.
* `static`: Configure the interface statically. The following additional keys
  must be present:
  * `addr`: IPv6 interface address, in `address/mask` format.

Gateways to be configured using IPv6 are specified as follows:

    {
        "type": "inet",
        "addr": <string>
    }

* _addr_: The IPv6 address of the default gateway.

## blk: Block devices

    "blk": {
        "<name>": {
            "type": <string>,
            "path": <string>
        },
        ...
    }
    ...

Configures a block device:

* _name_: The name of the block device to be configured in `/dev`.
* _type_: One of `etfs` or `vnd`.
* _path_: Type-specific, see below.

### etfs: Block device backed by rump_etfs host device

A _type_ of `etfs` registers the block device `/dev/<name>` as a `rump_etfs(3)`
device, with the host path _path_. 

ETFS devices are used by the rumprun unikernel to access storage on the Xen
platform. _path_ is specified as `blkfront:<device>`, where _device_ is the Xen
disk/partition to attach to.  Rumprun supports the following values for
_device_:

* `xvd[a-z][0-9]`: Xen virtual disk (PV guests)
* `sd[a-z][0-9]`: SCSI (HVM guests)
* `hd[a-z][0-9]`: IDE or AHCI (HVM guests)

For details refer to the [Xen Guest Disk (VBD)
specification](http://xenbits.xen.org/docs/unstable/misc/vbd-interface.txt).

### vnd: Loop-back block device

A _type_ of `vnd` configures the block device `/dev/<name>` as a loop-back
device backed by the file specified by _path_, using the vnode disk driver. 

_name_ must be specified as `vnd<unit>`, for example: `vnd0`.

## mount: Mount filesystems

    "mount": {
        "<mountpoint>": {
            "source": <string>,
            "path": <string>,
            "options": <reserved>
         },
         ...
    }

Mounts a filesystem:

* _mountpoint_: The path to mount the filesystem at. The directory hierarchy for
  _mountpoint_ will be created if it does not exist.
* _source_: The source for the filesystem. One of `kernfs` or `blk`.
* _path_: For `blk`, the path to the block device to mount.
* _options_: Reserved for future use by filesystem mount options.

### blk: Filesystem backed by block device

With a _source_ of `blk`, rumprun will mount the filesystem from the rump kernel
block device specified by _path_, on _mountpoint_.

The following filesystem types will be tried in succession: `ffs`, `ext2fs` and
`cd9660`. If the filesystem is not one of the supported types, configuration
will fail.

### kernfs: Virtual kernel filesystem

With a _source_ of `kernfs`, rumprun will mount the virtual `kernfs` filesystem
on _mountpoint_. The value of _path_ is ignored and need not be specified.

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
