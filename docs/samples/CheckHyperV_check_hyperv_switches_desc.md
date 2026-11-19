#### About `check_hyperv_switches`

`check_hyperv_switches` lists every Hyper-V virtual switch on the host as one
record, read from the `root\virtualization\v2` WMI namespace
(`Msvm_VirtualEthernetSwitch` and its ports). It is the native equivalent of
`Get-VMSwitch`.

`type` is worked out the way `Get-VMSwitch` reports `SwitchType`, from what
the switch's own ports connect to:

| `type`     | Meaning |
|------------|---------|
| `external` | Bound to a physical network adapter (wired or Wi-Fi). `management_os` says whether the host shares that adapter. |
| `internal` | No physical adapter, but the host has a virtual adapter on it: host and virtual machines can talk. |
| `private`  | Neither: only the virtual machines on this host can talk over it. |

The defaults warn on a switch whose `state` is anything but `enabled` or
`not_applicable` and go critical on one whose `health` is not `ok`. A switch
has no power state of its own, and Hyper-V reports most switches as
`not_applicable`, so a threshold written as `state != 'enabled'` would warn on
a healthy host. `ports` counts every port on the switch (each
connected virtual machine network adapter, plus the external and host ports)
and is emitted as perfdata.

A typical use is pinning the switches a host must have, so a switch deleted
or recreated by hand (which drops every VM connected to it) is noticed:
filter on the names, set `empty-state=critical`, and the check fails when
the switch disappears.

Without the Hyper-V role the namespace does not exist and the check reports
UNKNOWN with a message saying so; with the role installed but the *Hyper-V
Virtual Machine Management* service (vmms) stopped, the message points at
the service instead. A host with the role and no switches reports
`No virtual switches found` in the `empty-state` (UNKNOWN by default).
