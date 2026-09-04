# Security notices

This page is the running record of security-relevant changes in NSClient++ —
both published advisories (with their CVE / GHSA identifiers) and security
**hardening** that changed behaviour but was not assigned a CVE. It complements
the GitHub [Security Advisories](https://github.com/mickem/nscp/security/advisories)
tab, which lists only *published* advisories and so does not capture hardening
changes.

- To **report** a vulnerability, follow the
  [security policy](https://github.com/mickem/nscp/blob/main/SECURITY.md).
- For the concrete operator actions a given release requires, see
  [Upgrading](../setup/upgrading.md).

Use the filter below to narrow the page to your setup: pick the version you
are coming from and tick the modules your `nsclient.ini` enables. The
selection is shared with the Upgrading page and remembered in your browser.

<!--
  Contributors: do not add notices to this file. Each notice is its own file
  under docs/security/ (see docs/security/README.md); the markers below are
  expanded from those files when the site is built.
-->

<!-- notes:filter -->

---

## Published advisories

<!-- notes:advisories -->

---

## Hardening changes (no CVE)

Security-relevant changes that are handled as defense-in-depth / consistency
hardening rather than assigned a CVE are listed here as they ship, newest
first, alongside the release that contains them.

<!-- notes:hardening -->
