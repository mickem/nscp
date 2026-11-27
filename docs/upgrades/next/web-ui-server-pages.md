---
icon: "🔧"
modules: [WEBServer, NSCAServer, NSClientServer, NCPAServer, CheckMKServer, IcingaAgent]
action: none
---
**Web UI: settings pages for the servers.** Nothing to do. The module pages of
NSCAServer, NSClientServer, NCPAServer, CheckMKServer, WEBServer and IcingaAgent
get a page of their own, built like the NRPE one: every setting of the module on
one page (the rarely changed ones under *Advanced*), a *Default* / *Local*
switch on the settings the agent can also read from `[/settings/default]`,
the exact changes listed before they are saved, and *Show config* for an INI
fragment to copy to another agent or a fleet bundle. Passwords, tokens and
tickets are never shown back, only whether one is set.

Each page says plainly what weakens it: NSCA results accepted unencrypted (or
under `xor`; the NSCA page offers only the secure ciphers - AES, Twofish,
Serpent - and, as NSCA has its own
encryption, no TLS - other ciphers and TLS are set in expert mode), check_nt's password in clear (check_nt has no TLS, so its page offers none and
says when the agent's own TLS default keeps check_nt out), an NCPA or NSClient
listener that refuses everything for want of a token or password, NCPA
arguments reaching external scripts, anonymous web access, a missing
certificate meaning plain HTTP, and an Icinga agent that trusts its first
parent unverified. Saving is held back where the agent would refuse to listen or start:
NSCA encryption without a key, TLS without its certificate or CA, an Icinga
parent host without its endpoint name.

The check_mk local and MRPE entries, and the web server's users, stay as
lists below their page; web users no longer offer templates. The web server's
roles get an editor of their own: each role's grants picked by area (with the
area's `*` wildcard where it has one), the users holding it, a warning on roles
that can change the agent or run arbitrary commands, and the built-in roles as
starting points. The passive-results options show only once the cache is on. check_nt's allowed commands are picked from
the groups and commands the agent knows. check_mk offers the same three
transport modes as NRPE - two-way TLS, one-way TLS, no TLS - and,
as on the NRPE page, a stored configuration that matches none of them is shown
as *Custom* and left alone.
