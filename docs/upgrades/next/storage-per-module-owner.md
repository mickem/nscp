---
icon: "🗄️"
modules: [core, WEBServer, CheckDisk, CheckEventLog, CheckLogFile]
action: none
---
**Stored module state (`nsclient.db`) is now kept per module.**
The core store that modules keep state in across restarts — the WEB server's
login sessions, CheckDisk's trends, CheckEventLog's and CheckLogFile's
bookmarks — resolved every request to the first module loaded, so all modules
shared one namespace and two of them using the same context and key could
overwrite each other's entries. Entries are now owned by the module that
wrote them (by its alias when it was loaded with one, so two instances of a
module no longer share state either), and `nsclient.db` is written as file
version 2. Nothing to do: on the first start after the upgrade, each module
takes over the entries it stored before the moment it first reads them, so web
sessions and bookmarks survive. A downgrade to an older version after that
first start loses this state once (sessions log out, bookmarks start over):
the older agent only finds the entries of whichever module it loads first.
