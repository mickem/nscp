---
icon: "🔧"
modules: [packaging]
action: none
---
**The Windows installer creates `scripts\custom\` again, and always.** Nothing
to do — it is a folder to put your own scripts in, created empty.

It used to exist and was lost when a batch of outdated sample scripts was
removed. Unlike the shipped examples it is not part of the *Scripts*
(`SampleScripts`) installer feature, so `REMOVE=SampleScripts` no longer leaves
an installation with no `scripts\` directory at all — which previously meant an
`[/attachments]` entry, or anything else writing a script, had nowhere to land.

Nothing is installed into it, so an uninstall removes it again only if you have
left it empty.
