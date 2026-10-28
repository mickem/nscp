---
icon: "📊"
modules: [CheckSystem]
action: conditional
---
**`check_process`: `pagefile` and `peak_pagefile` no longer report each
other's value.** On Windows, `pagefile` read the peak page file usage and
`peak_pagefile` the current one (their descriptions were swapped to match).
`pagefile` (perfdata `<process> pf_use`) is now the current page file usage and
`peak_pagefile` (`<process> ppf_use`) the peak. Nothing to do unless a
`warning`, `critical`, `filter` or syntax uses one of these keywords: such a
threshold now measures what its name says, so one written against the old
behaviour (a "current" limit on `peak_pagefile`, say) should be swapped to the
other keyword. Graphs of the two perf series flip at the upgrade.
