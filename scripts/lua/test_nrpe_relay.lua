-- Protobuf-free NRPE round-trip test.
--
-- It stands up a local NRPE server and an NRPE client target, then uses the new
-- core:query_target(target, command, args) API to relay a check over NRPE and
-- assert the result. This exercises the full NRPE client + server + packet
-- parser path in-process, which is a good leak/UB target under the sanitizer build.
--
-- Replaces the old protobuf-based test_nrpe.lua (the Lua protobuf bindings are
-- disabled); query_target() is the API that makes this expressible without them.
local test = require("test_helper")

local NrpeRelayTest = {}

function NrpeRelayTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', 'test_checkhelpers', 'CheckHelpers')
	conf:set_string('/modules', 'test_nrpe_server', 'NRPEServer')
	conf:set_string('/modules', 'test_nrpe_client', 'NRPEClient')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_nrpe_relay', 'test_nrpe_relay.lua')

	-- Local NRPE server (insecure = no cert, relaxed ciphers for legacy interop).
	conf:set_string('/settings/NRPE/test_nrpe_server', 'port', '15666')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'allow arguments', 'true')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'insecure', 'true')

	-- NRPE client target 'good' pointing at the server, plus a relay handler that
	-- maps the local command 'check_remote' to the remote command 'check_ok'.
	conf:set_string('/settings/NRPE/test_nrpe_client/targets', 'good', 'nrpe://127.0.0.1:15666')
	conf:set_string('/settings/NRPE/test_nrpe_client/targets/good', 'insecure', 'true')
	conf:set_string('/settings/NRPE/test_nrpe_client/handlers', 'check_remote', 'check_ok')
end

function NrpeRelayTest:setup() end
function NrpeRelayTest:teardown() end

function NrpeRelayTest:run()
	local core = Core()
	local result = test.TestResult:new{message = 'NRPE relay via query_target'}

	local code, msg = core:query_target('good', 'check_remote', {})
	result:assert_equals(code, 'ok', 'check_ok relayed over NRPE to target "good" returns OK')
	result:add_message(msg ~= nil and msg ~= '', 'relayed response carries a message')

	return result
end

local instances = { NrpeRelayTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
