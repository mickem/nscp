---
icon: "🔧"
modules: [WEBServer, NCPAServer, NRPEServer, NSCAServer, NSClientServer, CheckMKServer, CheckSystemUnix]
action: none
---
**Linux/Unix: listener sockets are no longer inherited by the commands the agent starts.**
Nothing to do. The agent opened its listening sockets without close-on-exec,
so a command it started while they were open — `journalctl`, `dpkg-query` or
`systemctl` from `CheckSystemUnix` — inherited them and kept the port bound
for as long as it ran. A settings reload closes each listener and binds the
same port again; with such a command still running that bind could fail with
`Address already in use`, leaving the server down. Listeners (and the
connections the WEB server accepts) are now marked close-on-exec, and the
`CheckSystemUnix` command launcher closes every descriptor above stderr before
it runs the command, as the external-script launcher already did. Windows is
unaffected: sockets are not handed to child processes there.
