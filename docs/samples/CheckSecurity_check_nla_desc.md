#### About `check_nla`

`check_nla` reports the **Network Location Awareness** profile of each network
the machine knows about (the same classification Windows uses to pick a firewall
profile), via the COM `INetworkListManager` interface. It is a security-posture
check: confirm a domain-joined machine is on the `domain` category and not
accidentally treating a network as `private`/`public`.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `network` | string | Network name. |
| `category` | string | `public`, `private` or `domain`. |
| `connected` | bool | True if the network is currently connected. |

There is no default threshold — assert the expected posture, e.g.
`crit=connected = 1 and category != 'domain'`. **Windows only**; on other
platforms it returns UNKNOWN with a clear message (Linux has no equivalent
network-profile concept).
