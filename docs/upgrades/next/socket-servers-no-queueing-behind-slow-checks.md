---
icon: "⏱️"
modules: [NRPEServer, NSCAServer, CheckMKServer, NSClientServer]
action: none
---
**NRPE, NSCA, check_mk and NSClient servers: a request no longer waits for an unrelated slow check.**
Nothing to do. Each connection serialised its work on an asio strand, and the
kind of strand the socket servers used shares a fixed pool of 193
implementations between all connections - so a new request could land on the
strand of one whose check was still running and wait for that check to finish
before it was even read. About one request in a few hundred did, whenever a
long-running check was in flight, which a monitoring server sees as an
occasional `Socket timeout` on a check that normally answers at once. The
accept loop could be held up the same way. Every connection, and the accept
loop, now has a strand of its own.
