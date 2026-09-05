---
icon: "🔢"
modules: [filters]
action: conditional
---
**An unknown unit in `format_bytes()` is now reported instead of rendering
nonsense.** `format_bytes(used, 'gb')` used to render `1.27055e-10` because
the unit comparison was case sensitive, and any misspelled unit rendered
`value/1024^7`. Lowercase units now work, and a unit that names nothing (say
`'ZB'`) makes the check report `Filter processing failed: format_bytes
failed: Unknown byte unit: ZB`. A syntax string with such a typo returns
UNKNOWN rather than a quietly wrong number - fix the unit, or the check will
stay UNKNOWN.
