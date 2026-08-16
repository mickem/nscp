**Check that the rules you depend on are in effect (Windows)**

Without `expect=` the check just inventories the rule set; the default critical
only fires for an expected rule that is not in effect.

```
check_firewall_rules
L        cli OK: 631 rule(s) checked, all as expected|'count'=631;0;0
```

```
check_firewall_rules "expect=Remote Desktop - User Mode (TCP-In)"
L        cli OK: 631 rule(s) checked, all as expected|'count'=631;0;0
```

A rule that was deleted and one that was merely switched off both fail, with
different wording so you know which fix is needed:

```
check_firewall_rules "expect=NSCP no such rule zzz" "expect=Distributed Transaction Coordinator (TCP-in)"
L        cli CRITICAL: NSCP no such rule zzz: not in effect: no rule with this name, Distributed Transaction Coordinator (TCP-in): not in effect: the rule exists but is disabled|'count'=633;0;0
```

Rule names are **localized**: on a Swedish machine the RDP rule above is
`Fjärrskrivbord - användarläge (TCP-In)`. Take the names from the machine you
are checking (`Get-NetFirewallRule | Select DisplayName`), or match on `group`
instead.

**Find inbound allow rules that restrict neither address nor port**

`any_any` is offered as a keyword rather than a default threshold — a normal
Windows client has a hundred of them (every packaged app rule), so alerting on
them out of the box would be pure noise. On a server, where the rule set is
curated, it is a useful thing to bound.

```
check_firewall_rules "filter=any_any = 1" "top-syntax=${count} unrestricted inbound allow rule(s)" "ok-syntax=${count} unrestricted inbound allow rule(s)"
L        cli 120 unrestricted inbound allow rule(s)|'count'=120;0;0
```

To bound how many there may be, threshold on `count` — and give the check a
summary top syntax while you are at it, or the alert will list every rule that
matched the filter:

```
check_firewall_rules "filter=any_any = 1" "warning=count > 5" "top-syntax=${status}: ${count} unrestricted inbound allow rule(s)"
L        cli WARNING: 120 unrestricted inbound allow rule(s)|'count'=120;5;0
```

**Alert on any inbound allow rule that reaches a sensitive port**

```
check_firewall_rules "critical=enabled = 1 and direction = 'in' and action = 'allow' and local_ports like '3389'"
L        cli CRITICAL: Open RDP to the world: in allow tcp port 3389 (unrestricted), all
```

**Inventory what a rule actually does**

An unrestricted address or port field reads `*`, matching what the firewall UI
shows as "Any".

```
check_firewall_rules "filter=name = 'File and Printer Sharing (SMB-In)'" "top-syntax=${list}" "ok-syntax=${list}" "detail-syntax=${name}: ${direction}/${action} proto=${protocol} lports=${local_ports} remote=${remote_addresses} profiles=${profiles}"
L        cli File and Printer Sharing (SMB-In): in/allow proto=tcp lports=445 remote=* profiles=domain,private
```

**Count only what is switched on**

```
check_firewall_rules "filter=enabled = 1" "top-syntax=${count} enabled rules" "ok-syntax=${count} enabled rules"
L        cli 416 enabled rules|'count'=416;0;0
```

**On non-Windows platforms**

```
check_firewall_rules
L        cli UNKNOWN: check_firewall_rules is not supported on this platform (Windows firewall rules only)
```
