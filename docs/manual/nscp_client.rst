***********
Client Mode
***********

The main goal of client mode is to do things you would normally do remotely, locally. For instance you would normally run CheckProcess? via check_nrpe from your Nagios machine, but you can do the exact same using nscp client --query CheckProcess. This can be useful in many instances such as debugging commands (locally) or executing check_nrpe from a windows machine etc.

Client has three (ish) modes of operation:


* '''--query''' To execute a query against a module.
* '''--exec''' To execute a command against a module.
* '''--submit''' To submit a response via any of the passive protocols.