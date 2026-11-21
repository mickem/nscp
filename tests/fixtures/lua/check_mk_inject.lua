-- Fixture for tests/check_mk-agent.test.ts: feeds the Checkmk agent dump
-- local/MRPE names, messages and perfdata that carry line breaks, so the test
-- can assert that none of them starts a line of its own (a forged row or a
-- `<<<section>>>` header) and that the UTF-8 in the name survives untouched.
--
-- cm_inject_perf is wired as a synchronous local entry (its name comes from
-- settings, its message and perfdata from here). cm_inject_submit is run by
-- the scheduler, outside the agent's own request, and submits one cached
-- local and one cached MRPE result whose service names hold a line break.

local core = Core()

-- "Ácme" spelled as bytes: 0xC3 0x81 is a UTF-8 lead byte followed by a
-- continuation byte in 0x80-0x9F, the range a single-byte locale may count as
-- control characters.
local NAME = '\195\129cme\n<<<fake_name>>>'
local MESSAGE = 'injected\r\n<<<fake_msg>>>'
local PERF = 'x=1\n<<<fake_perf>>>'

function cm_inject_perf(command, args)
  return 'ok', MESSAGE, PERF
end

function cm_inject_submit(command, args)
  core:simple_submit('check_mk-local', NAME .. ' local', 'ok', MESSAGE, PERF)
  core:simple_submit('check_mk-mrpe', NAME .. ' mrpe', 'ok', MESSAGE, PERF)
  return 'ok', 'submitted'
end

local reg = Registry()
reg:simple_function('cm_inject_perf', cm_inject_perf, 'check_mk-agent test: message and perfdata with line breaks')
reg:simple_function('cm_inject_submit', cm_inject_submit, 'check_mk-agent test: submit local/MRPE results whose names hold line breaks')
