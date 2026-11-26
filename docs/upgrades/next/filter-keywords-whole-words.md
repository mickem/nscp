---
icon: "🔧"
modules: [filters]
action: conditional
---
**`and`, `or`, `in` and the word operators only match as whole words.** Like
`not` before them, these keywords - `and`, `or`, `in`, `le`, `lt`, `eq`, `ne`,
`ge`, `gt`, `like`, `regexp` - were matched as prefixes, so a filter keyword
whose name started with one could be split: `x next` was read as `x ne xt`,
and `a > 1 order > 2` was accepted as `a > 1 or (der > 2)` and silently
checked something other than what it said. Such names now parse as the check
keyword they name, and an expression that only parsed because a word was split
is reported as invalid. `not like`, `not regexp` and `not in` also accept more
than one space between the two words. Nothing to do unless a `filter`,
`warning` or `critical` relied on such a split.
