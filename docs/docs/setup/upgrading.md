# Upgrading

What to do when upgrading NSClient++, newest release first. Most upgrades are
in place — defaults are preserved and the default install is usually unaffected
— but the items below change observable behaviour or want a configuration
touch. Read the entries between the version you are on and the version you are
moving to.

Each entry carries an icon for the area it touches. 🔒 is the one that
matters: those entries are security-relevant, and the
[Security notices](../security/notices.md) page tracks them in one place. An
entry badged <span class="note-badge note-badge--required">Action required</span>
needs a change on every installation that runs the module; one badged
<span class="note-badge note-badge--conditional">Check your setup</span> only
if you use the feature it describes; the rest are informational. Full
per-release detail lives in each
[GitHub release](https://github.com/mickem/nscp/releases).

Use the filter below to narrow the list to your setup: pick the version you
are coming from and tick the modules your `nsclient.ini` enables, and only the
entries that concern that installation stay visible. The selection is shared
with the Security notices page, remembered in your browser and reflected in
the page address, so a filtered view can be bookmarked or shared.

<!--
  Contributors: do not add entries to this file. Each entry is its own file
  under docs/upgrades/<version>/ (see docs/upgrades/README.md); the two markers
  below are expanded from those files when the site is built.
-->

<!-- notes:filter -->

---

<!-- notes:upgrades -->
