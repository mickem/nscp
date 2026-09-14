---
icon: "📅"
modules: [CheckSystemUnix]
action: conditional
---
**`check_installed_software` takes dpkg install dates from `dpkg-query`.**
Nothing to do on a current Debian or Ubuntu host: the date is the same as
before, now read from `dpkg-query`'s `db-fsys:Last-Modified` field instead of
from dpkg's internal database. That field needs dpkg 1.19.3 or later; with an
older dpkg `install_date` is now left unset, so an expression on it such as
`warning=install_date > -7d` no longer matches anything there. The check also
returns UNKNOWN when dpkg cannot read a package's file list for a reason other
than the file being missing, where it used to leave only that package undated.
