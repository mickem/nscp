-- Minimal, protobuf-free Lua unit test used to validate that the Lua unit-test
-- harness works end-to-end under the sanitizer build:
--   install (sets config) -> reload (registers lua_unittest) -> run -> leak check.
-- It exercises the lua -> core -> lua simple_query dispatch path entirely in-process,
-- with no dependency on the (hard-disabled) Lua protobuf bindings or any network module.

local function selfquery(command, args)
  return 'ok', 'hello from lua selfquery'
end

local function lua_unittest_handler(command, args)
  local core = Core()
  local code, msg, perf = core:simple_query('lua_selfquery', {})
  if msg == 'hello from lua selfquery' then
    core:log('info', 'OK: 1 test(s) successfull')
    return 'ok', '1 test(s) successfull'
  end
  core:log('error', 'selfcheck FAILED, got: ' .. tostring(msg))
  return 'crit', 'selfcheck failed'
end

local reg = Registry()
reg:simple_function('lua_selfquery', selfquery, 'self query used by test_selfcheck')
reg:simple_query('lua_unittest', lua_unittest_handler, 'lua self-check unit test')

function main(args)
  local conf = Settings()
  conf:set_string('/modules', 'luatest', 'LUAScript')
  conf:set_string('/settings/luatest/scripts', 'test_selfcheck', 'test_selfcheck.lua')
  return 'ok', 'installed'
end
