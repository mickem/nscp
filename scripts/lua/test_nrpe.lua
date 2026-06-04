-- Protobuf-free Lua port of scripts/python/test_nrpe.py.
--
-- The Python NRPE acceptance test drives a full NRPE client+server round trip
-- across a matrix of "SSL on/off" x "payload length" x "Nagios state", asserting
-- that an arbitrary command result (state + message) survives the trip. It did so
-- via the protobuf plugin API (a custom server-side handler keyed by a per-request
-- UUID).
--
-- This port keeps that coverage without protobuf by combining two pieces:
--   1. A server-side Lua handler `check_echo` registered on this same NSCP
--      instance. It simply echoes back the state and message it is handed as
--      arguments, so whatever we send must come back unchanged if the transport
--      is faithful.
--   2. The new core:query_forward(forward_command, target, command, args) binding.
--      It sets the request header command to a relay's "forward as-is" command
--      (here "nrpe_forward"), which ships our payload command + arguments onto the
--      wire untouched - unlike a handler alias, whose mapped command is not
--      currently forwarded (see the i_do_query custom_command TODO in
--      include/client/command_line_parser.cpp). So `check_echo <state> <message>`
--      actually reaches the server and dispatches back into this script.
--
-- Looping the round trip over SSL on/off and a spread of payload lengths also
-- exercises the client, the SSL layer and the v2 packet (de)serializer across a
-- range of buffer sizes - the classic NRPE packet is padded to the configured
-- "payload length" on the wire regardless of message size (see
-- net/nrpe/server/parser.hpp get_packet_length_v2), which is where the
-- memory/UB bugs under ASan/LSan/UBSan tend to live.
--
-- Replaces the old protobuf-based test_nrpe.lua (the Lua protobuf bindings are
-- disabled). A minimal single-shot smoke version lives in test_nrpe_relay.lua.
local test = require("test_helper")

local SERVER = 'test_nrpe_server'
local CLIENT = 'test_nrpe_client'
local TARGET = 'valid'
local PORT = '15666'
-- The relay's "forward as-is" command: routes to the NRPE client and ships the
-- payload command + arguments onto the wire verbatim (modules/NRPEClient/module.json).
local FORWARD = 'nrpe_forward'
-- Command we register below and forward over NRPE; it echoes its arguments back.
local ECHO = 'check_echo'

-- SSL on/off and a spread of payload lengths to flex the packet buffers.
local SSL_MODES = {true, false}
local LENGTHS = {1024, 4096, 65536}

-- Nagios states to round-trip. These are the canonical strings core returns
-- (see lua push_code), so a faithful trip yields back exactly what we sent.
local STATES = {'ok', 'warning', 'critical', 'unknown'}

-- Server-side handler: echo the requested state and message straight back.
-- args[1] = state string, args[2] = message. Registered once at script load so
-- the NRPE server can dispatch the forwarded `check_echo` to it.
local function check_echo(command, args)
	return args[1] or 'unknown', args[2] or '', ''
end
Registry():simple_function(ECHO, check_echo, 'NRPE round-trip echo (test helper)')

local NrpeTest = {}

function NrpeTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', SERVER, 'NRPEServer')
	conf:set_string('/modules', CLIENT, 'NRPEClient')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_nrpe', 'test_nrpe.lua')

	-- Local NRPE server: insecure (no cert, relaxed ciphers) so we can speak SSL
	-- without provisioning certificates; arguments allowed so check_echo receives
	-- the state/message we forward. SSL and payload length are (re)set per
	-- iteration in reconfigure().
	conf:set_string('/settings/NRPE/' .. SERVER, 'port', PORT)
	conf:set_string('/settings/NRPE/' .. SERVER, 'insecure', 'true')
	conf:set_string('/settings/NRPE/' .. SERVER, 'allow arguments', 'true')

	-- NRPE client target pointing at the server.
	conf:set_string('/settings/NRPE/' .. CLIENT .. '/targets', TARGET, 'nrpe://127.0.0.1:' .. PORT)
	conf:set_string('/settings/NRPE/' .. CLIENT .. '/targets/' .. TARGET, 'insecure', 'true')
end

function NrpeTest:setup() end
function NrpeTest:teardown() end

-- Apply one (ssl, length) combination to both modules and reload them.
function NrpeTest:reconfigure(ssl, length)
	local conf = Settings()
	local sbool = ssl and 'true' or 'false'

	conf:set_string('/settings/NRPE/' .. SERVER, 'use ssl', sbool)
	conf:set_string('/settings/NRPE/' .. SERVER, 'payload length', tostring(length))

	local tpath = '/settings/NRPE/' .. CLIENT .. '/targets/' .. TARGET
	conf:set_string(tpath, 'use ssl', sbool)
	conf:set_string(tpath, 'payload length', tostring(length))

	local core = Core()
	core:reload(SERVER)
	core:reload(CLIENT)
	-- Give the freshly reloaded server a moment to rebind its listener before we
	-- fire the first request at it (mirrors the Python test's settle/poll loop).
	nscp.sleep(500)
end

-- Forward `check_echo state message` over NRPE and assert both survive the trip.
function NrpeTest:test_state(ssl, length, state)
	local result = test.TestResult:new{message = string.format('ssl=%s, length=%d, state=%s', tostring(ssl), length, state)}
	-- Short, separator-free message so it round-trips intact (well under any
	-- payload length and free of NRPE argument separators).
	local message = string.format('msg_%s_%d', state, length)

	local core = Core()
	local code, msg = core:query_forward(FORWARD, TARGET, ECHO, {state, message})

	result:assert_equals(code, state,
		string.format('state survives NRPE round trip (ssl=%s, len=%d)', tostring(ssl), length))
	result:assert_equals(msg, message,
		string.format('message survives NRPE round trip (ssl=%s, len=%d)', tostring(ssl), length))
	return result
end

-- Run all states through one (ssl, length) configuration.
function NrpeTest:test_one(ssl, length)
	local result = test.TestResult:new{message = string.format('ssl=%s, length=%d', tostring(ssl), length)}
	self:reconfigure(ssl, length)
	for _, state in ipairs(STATES) do
		result:add(self:test_state(ssl, length, state))
	end
	return result
end

function NrpeTest:run()
	local result = test.TestResult:new{message = 'NRPE client/server round trip via query_forward'}
	for _, ssl in ipairs(SSL_MODES) do
		for _, length in ipairs(LENGTHS) do
			result:add(self:test_one(ssl, length))
		end
	end
	return result
end

local instances = { NrpeTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
