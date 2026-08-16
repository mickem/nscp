#### About `check_firewall_rules`

`check_firewall_rules` reports the Windows firewall **rules** — one row per rule
— where [`check_firewall`](#check_firewall) reports only whether each profile is
switched on. It answers the two questions the profile check cannot: is the rule
I depend on still there and enabled, and is there an inbound allow rule that
restricts nothing.

Rules are read through `INetFwPolicy2::Rules`, the same store
`Get-NetFirewallRule` uses, in a single pass across all profiles. No WMI needed.

Keywords (one row per rule):

| Keyword | Type | Meaning |
|---|---|---|
| `name` | string | Rule name as it appears in the firewall. |
| `description` | string | Rule description. |
| `group` | string | Rule group, e.g. `Remote Desktop` — often a resource reference like `@FirewallAPI.dll,-28752` for built-in rules. |
| `direction` | string | `in` or `out`. |
| `action` | string | `allow` or `block`. |
| `protocol` | string | `tcp`, `udp`, `icmpv4`, `icmpv6`, `any`, or the raw protocol number (e.g. `41` for IPv6). |
| `profiles` | string | `all`, or a comma separated subset of `domain`, `private`, `public`. |
| `local_ports` / `remote_ports` | string | Ports the rule covers; `*` for any. |
| `local_addresses` / `remote_addresses` | string | Addresses the rule covers; `*` for any. |
| `application` | string | Program the rule is bound to (empty when it is not program specific). |
| `service` | string | Service the rule is bound to. |
| `state` | string | One-line summary of what the rule does; what the default output shows. |
| `enabled` | bool | The rule is switched on. |
| `present` | bool | False only for an `expect=` name that no enabled rule satisfies — this is the default critical. |
| `expected` | bool | The rule matched one of the `expect=` names. |
| `any_remote` | bool | Accepts traffic from any remote address. |
| `any_port` | bool | Covers any local port. |
| `any_any` | bool | An **enabled inbound allow** rule that restricts neither. |
| `edge_traversal` | bool | Accepts traffic that traversed a NAT device. |

Options:

| Option | Repeatable | Meaning |
|---|---|---|
| `expect` | yes | A rule that must exist and be enabled, matched on the exact name, case insensitively. |

Windows leaves a scope field empty where the firewall UI shows "Any"; the check
normalises those to `*`, so `local_ports = '*'` matches every unrestricted rule
rather than only some of them.

**Asserting a rule exists.** `expect=` fails whether the rule was deleted or
merely switched off, and says which: *"no rule with this name"* versus *"the rule
exists but is disabled"*. Windows allows several rules to share a name (commonly
one per profile); one **enabled** copy satisfies the expectation, which is how
the firewall itself behaves. Rule names are localized — on a Swedish machine the
RDP rule is `Fjärrskrivbord - användarläge (TCP-In)` — so take the names from the
machine you are checking rather than from an English reference.

**The any-any rule.** `any_any` is deliberately restricted to *inbound allow*
rules: outbound traffic is unrestricted by default on Windows, and a wide
*block* is the opposite of a finding. It is offered as a keyword, not imposed as
a threshold, because a normal Windows client legitimately has a hundred of them
(every packaged app gets one) — alerting by default would be noise. On a curated
server rule set, `filter=any_any = 1` with a `count` threshold is a good bound.

Default thresholds: **critical** when `present = 0`, which only fires when
`expect=` is used; nothing else alerts on its own. The default top syntax lists
only the *problem* rules, because a normal host has several hundred rules and
listing them all would be unreadable; `count` perfdata carries how many rules
matched the filter. empty-state is **OK**. **Windows only.**
