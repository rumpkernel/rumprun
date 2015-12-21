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

# Configuration format

Rumprun unikernel configuration is defined using JSON:

    { <JSON expression> }

Configuration data must be encoded in UTF-8. Platform-specific methods (see
"Platform notes" below) are used to pass configuration data to the unikernel.
Configuration data which does not start with `{` will be ignored.

Given that a JSON object is by definition unordered, configuration keys will be
processed by rumprun in the following order:

1. _rc_: Program invocation
2. _env_: Environment variables
3. _hostname_: Kernel hostname
4. _blk_: Block device configuration
5. _mount_: Filesystem mounts
6. _net_: Network configuration
  1. _interfaces_: Interfaces
  2. _gateways_: Default gateways

A rumprun unikernel does not _require_ any configuration to be passed for it to
boot.

## rc: Program invocation

    "rc": [
         {
             "bin": <string>,
             "args": [ <string>, ... ],
             "runmode": "& OR |"
         },
         ...
    ]

Each element of `rc` describes a single program, **in the order in which they
are baked into the unikernel image**.

If the `rc` key is not specified in the configuration, only the first
multibaked program in the unikernel will be invoked.

* _bin_: The name of the program. Passed to the program as `argv[0]`.
* _args[]_: Arguments for the program. Passed to the program as `argv[1..N]`.
* _runmode_: Defines how the program will be invoked. _Optional_
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
            <source-specific keys>
         },
         ...
    }

Mounts a filesystem:

* _mountpoint_: The path to mount the filesystem at. The directory hierarchy for
  _mountpoint_ will be created if it does not exist.
* _source_: The source for the filesystem. One of `blk`, `kernfs` or `tmpfs`.

### blk: Filesystem backed by block device

    "path": <string>

A _source_ of `blk` mounts a filesystem from the block device specified by
_path_ on _mountpoint_.

The following filesystem types will be tried in succession: `ffs`, `ext2fs` and
`cd9660`. If the filesystem is not one of the supported types, configuration
will fail.

### kernfs: Virtual kernel filesystem

A _source_ of `kernfs` mounts the virtual `kernfs` filesystem on _mountpoint_.

### tmpfs: In-memory filesystem

    "options": {
        "size": <string>
    }

A _source_ of `tmpfs` mounts an in-memory `tmpfs` filesystem on _mountpoint_.

* _size_: The maximum size of the filesystem, specified as `<size>k|M|G`.
  Defaults to 1 MB.

# Platform notes

## hw/x86: Bare metal, KVM and other HVM hypervisors on x86

When targetting the _hw_ platform on x86, rumprun uses the multiboot protocol.

Configuration data is passed to the unikernel as the _first_ multiboot module.

### Example configuration

The following is an example configuration for a rumprun unikernel running
`mathopd`:

    {
      "net": {
         "interfaces": {
           "vioif0": {
             "addrs": [ 
               { "type": "inet", "method": "static", "addr": "10.0.120.10/24" }
             ]
           }
         },
         gateways": [
           { "type": "inet", "addr": "10.0.120.1" }
         ]
      },
      "mount": {
        "/etc": {
          "source": "blk",
          "path": "/dev/ld0a"
        },
        "/data": {
          "source": "blk",
          "path": "/dev/ld1a"
        },
        "/tmp": {
          "source": "tmpfs",
          "options": { "size": "1M" }
        }
      },
      "rc": [
        {
          "bin": "mathopd",
          "args": [ "-n", "-t", "-f", "/data/mathopd.conf" ]
        }
      ],
      "env": {
        "FOO": "BAR",
        "BAZ": "QUUX"
      }
    }

The following example command could be used to launch the unikernel using KVM:

    qemu-system-x86_64 -net nic,model=virtio -net bridge,br=rrbr0 \
        -enable-kvm -cpu host \
        -drive if=virtio,file=stubetc.iso \
        -drive if=virtio,file=data.iso \
        -m 64 \
        -vga none -nographic \
        -kernel mathopd.bin -initrd mathopd.json 

## xen/x86: Paravirtualized Xen on x86

Configuration data must be written to the domain-specific `rumprun/cfg` key in
Xenstore. In practice, due to the way the Xen toolstack works, this means:

1. Creating the domain in a paused state, in order to obtain the _domid_.
2. Writing configuration data to `/local/domain/<domid>/rumprun/cfg`.
3. Unpausing the domain.

### Example configuration

The following is an example configuration for a rumprun unikernel on Xen:

    {
      "net": {
        "interfaces": {
          "xenif0": {
            "create": true,
            "addrs": [
              { "type": "inet", "method": "static", "addr": "10.0.120.10/24" }
            ], 
          }
        }
      },
      "blk": {
        "xbd0": {
          "type": "etfs", 
          "path": "blkfront:xvda"
        },
      },
      "mount": {
        "/test": {
          "source": "blk", 
          "path": "/dev/xbd0",
        },
        "/kern": {
          "source": "kernfs"
        }
      }
      "rc": [ { "bin": "./wopr.bin" } ]
    }
