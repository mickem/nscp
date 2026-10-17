---
icon: "🆕"
modules: [CheckSystem]
action: none
---
**The `os` fact set now says whether a Windows host is in a domain or a
workgroup.** Nothing to do. Two new fields, `join` (`domain`, `workgroup` or
`standalone`) and `join_name` (the Active Directory domain as the join API
reports it, usually the DNS name such as `corp.example.com`, or the
workgroup), sit next to `domain`. They are the values `check_hostname` reports
as its `join` and `join_name` keywords. `domain` itself is unchanged: it was
and is the host's DNS domain (the primary DNS suffix), which a workgroup
computer can have too, so it never told you whether the host was joined. The
fields are Windows-only and are omitted elsewhere. As before, the `os` set is
only collected once it is turned on (#1672). See
[Facts](../concepts/facts.md#os-and-hardware).
