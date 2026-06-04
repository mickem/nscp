-- Protobuf-free Lua port of scripts/python/test_nsca.py.
--
-- Exercises a full NSCA client->server passive-result round trip in-process:
--   * an NSCAServer receives submissions and routes them to an inbox channel;
--   * a Lua simple_subscription on that channel captures what arrives;
--   * results are pushed through the NSCAClient via core:simple_submit (which
--     routes to the client's default target and out over the NSCA wire).
-- We then assert the submitted command, message and state survived the trip.
--
-- The Python original sweeps every cipher x {128,512,1024,4096} payload lengths
-- x 4 states x 3 targets. Per the task, this port keeps a deliberately small
-- matrix - a couple of representative ciphers and all four states at one length -
-- which is enough to prove the encrypt/decrypt + packet path under the sanitizer
-- build without a multi-minute run. No protobuf.
local test = require("test_helper")

local SERVER = 'test_nsca_server'
local CLIENT = 'test_nsca_client'
local PORT = '15667'
local OUTBOX = 'nsca_test_outbox'   -- channel the client listens on
local INBOX = 'nsca_test_inbox'     -- channel the server delivers to

-- A small, representative cipher set (not the full NSCA matrix) at one length.
local CIPHERS = {'none', 'aes'}
local LENGTH = 512
local STATES = {'ok', 'warning', 'critical', 'unknown'}

-- Captured inbox messages, keyed by submitted command (a per-submission uid).
local received = {}

-- Inbox sink: record the command/result/message the server delivered.
local function on_inbox(channel, command, result, lines)
	local message = nil
	for line, _perf in pairs(lines) do
		message = line
		break
	end
	received[command] = {result = result, message = message or ''}
	return 'ok', 'received'
end

Registry():simple_subscription(INBOX, on_inbox, 'NSCA inbox sink (test)')

local NscaTest = {}

function NscaTest:install(arguments)
	local conf = Settings()
	conf:set_string('/modules', SERVER, 'NSCAServer')
	conf:set_string('/modules', CLIENT, 'NSCAClient')
	conf:set_string('/modules', 'luatest', 'LUAScript')
	conf:set_string('/settings/luatest/scripts', 'test_nsca', 'test_nsca.lua')

	conf:set_string('/settings/NSCA/' .. SERVER, 'port', PORT)
	conf:set_string('/settings/NSCA/' .. SERVER, 'inbox', INBOX)

	-- The client listens on OUTBOX and relays to its default target.
	conf:set_string('/settings/NSCA/' .. CLIENT, 'channel', OUTBOX)
	conf:set_string('/settings/NSCA/' .. CLIENT .. '/targets', 'default', 'nsca://127.0.0.1:' .. PORT)
end

function NscaTest:setup() end
function NscaTest:teardown() end

-- Apply one cipher to both ends (shared password + length) and reload.
function NscaTest:reconfigure(cipher)
	local conf = Settings()
	local password = 'pwd-' .. cipher

	conf:set_string('/settings/NSCA/' .. SERVER, 'encryption', cipher)
	conf:set_string('/settings/NSCA/' .. SERVER, 'password', password)
	conf:set_string('/settings/NSCA/' .. SERVER, 'payload length', tostring(LENGTH))

	local tpath = '/settings/NSCA/' .. CLIENT .. '/targets/default'
	conf:set_string(tpath, 'encryption', cipher)
	conf:set_string(tpath, 'password', password)
	conf:set_string(tpath, 'payload length', tostring(LENGTH))

	local core = Core()
	core:reload(SERVER)
	core:reload(CLIENT)
	nscp.sleep(500)
end

-- Poll for an inbox delivery keyed by uid. nscp.sleep releases the Lua GIL, so
-- the server's inbox handler runs on its own thread while we wait here without
-- racing us for the lua_State.
function NscaTest:wait_for(uid)
	for _ = 1, 20 do
		if received[uid] then return received[uid] end
		nscp.sleep(200)
	end
	return nil
end

-- Submit one passive result and assert it round-trips faithfully.
function NscaTest:submit_one(cipher, state)
	local result = test.TestResult:new{message = string.format('NSCA %s/%s', cipher, state)}
	local uid = 'nsca_' .. cipher .. '_' .. state .. '_' .. test.random(8)
	local message = uid .. ' - hello'

	local accepted = Core():simple_submit(OUTBOX, uid, state, message, '')
	result:add_message(accepted, 'submission accepted')

	local got = self:wait_for(uid)
	if got == nil then
		result:add_message(false, 'inbox received submission ' .. uid)
		return result
	end
	result:add_message(true, 'inbox received submission')
	result:assert_contains(got.message, message, 'message survived the round trip')
	result:assert_equals(got.result, state, 'state survived the round trip')
	return result
end

function NscaTest:run()
	local result = test.TestResult:new{message = 'NSCA client/server round trip via simple_submit'}
	for _, cipher in ipairs(CIPHERS) do
		local sub = test.TestResult:new{message = 'cipher=' .. cipher}
		self:reconfigure(cipher)
		for _, state in ipairs(STATES) do
			sub:add(self:submit_one(cipher, state))
		end
		result:add(sub)
	end
	return result
end

local instances = { NscaTest }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
