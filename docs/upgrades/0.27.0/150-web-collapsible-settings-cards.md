---
icon: "🗂️"
modules: [WEBServer]
action: none
---
**The web UI's settings cards fold away, and the failed-check log can be configured where it is shown.**
The *Fact sets* card on the Facts page and the *Diagnostics tables* card on
the Diagnostics page now collapse under their title; whether a card is open is
remembered per browser. The Failed checks page gains a *Failed-check log
settings* card, folded by default, that lists every key of
`[/settings/core/failed checks]` with the same editor it has on the settings
page - how many entries to keep, which statuses count, which arguments to
record, and the metrics and diagnostics capture. As with the other cards, a
change takes effect when the configuration is saved and reloaded. The card is
not shown to a role that cannot read the settings. Nothing to do.
