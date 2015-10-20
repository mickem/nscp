test = require("test_helper")

TestExtScript = {
	requests = {}, 
	responses = {} 
}
function TestExtScript:install(arguments)
	local conf = nscp.Settings()
	
	conf:set_string('/modules', 'test_extscripts', 'CheckExternalScripts')
	conf:set_string('/modules', 'luatest', 'LUAScript')

	conf:set_string('/settings/luatest/scripts', 'test_nrpe', 'test_nrpe.lua')
	
	conf:set_string('/settings/NRPE/test_nrpe_server', 'port', '15666')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'inbox', 'nrpe_test_inbox')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'encryption', '1')

	conf:set_string('/settings/NRPE/test_nrpe_client/targets', 'nrpe_test_local', 'nrpe://127.0.0.1:15666')
	conf:set_string('/settings/NRPE/test_nrpe_client', 'channel', 'nrpe_test_outbox')
	--conf:save()
end

function TestExtScript:setup()
	local reg = nscp.Registry()
	reg:simple_query('check_py_nrpe_test_s', self, self.simple_handler, 'TODO')
	--reg:query('check_py_nrpe_test', self, self.handler, 'TODO')
end
function TestExtScript:teardown()
end

function TestExtScript:uninstall()
end

function TestExtScript:help()
end

function TestExtScript:init(plugin_id)
end

function TestExtScript:shutdown()
end

function TestExtScript:has_response(id)
	return self.responses[id]
end

function TestExtScript:get_response(id)
	msg = self.requests[id]
	if msg == nil then
		msg = TestMessage
		msg.uuid=id
		self.responses[id] = msg
		return msg
	end
	return msg
end

function TestExtScript:set_response(msg)
	self.responses[msg.uuid] = msg
end

function TestExtScript:del_response(id)
	self.responses[id] = nil
end
		
function TestExtScript:get_request(id)
	msg = self.requests[id]
	if msg == nil then
		msg = TestMessage
		msg.uuid=id
		self.requests[id] = msg
		return msg
	end
	return msg
end

function TestExtScript:set_request(msg)
	self.requests[msg.uuid] = msg
end

function TestExtScript:del_request(id)
	self.requests[id] = nil
end

function TestExtScript:simple_handler(command, args)
	local core = nscp.Core()
	msg = self:get_response(args[0])
	msg.got_simple_response = true
	self:set_response(msg)
	message = rmsg.message
	if args[1] then
		message = string.rep('x', args[1])
	end
	rmsg = self:get_request(args[0])
	return rmsg.status, message, rmsg.perfdata
end

function TestExtScript:handler(req)
	local msg = self:get_response(args[0])
	msg.got_response = true
	self:set_response(msg)
end

function TestExtScript:submit_payload(tag, ssl, length, payload_length, source, status, message, perf, target)
	local core = nscp.Core()
	local result = test.TestResult:new{message='Testing NRPE: '..tag..' for '..target}
	
	local msg = protobuf.Plugin.QueryRequestMessage.new()
	hdr = msg:get_header()
	hdr:set_recipient_id(target)
	hdr:set_command('nrpe_forward')
	host = hdr:add_hosts()
	host:set_address("127.0.0.1:15666")
	host:set_id(target)
	if target == 'valid' then
	else
		enc = host:add_metadata()
		enc:set_key("use ssl")
		enc:set_value(tostring(ssl))
		enc = host:add_metadata()
		enc:set_key("payload length")
		enc:set_value(tostring(length))
		enc = host:add_metadata()
		enc:set_key("timeout")
		enc:set_value('10')
	end

	uid = string.random(12)
	payload = msg:add_payload()
	payload:set_command('check_py_nrpe_test_s')
	payload:set_arguments(1, uid)
	if payload_length ~= 0 then
		payload:set_arguments(2, payload_length)
	end
	rmsg = self:get_request(uid)
	rmsg.status = status
	rmsg.message = message
	rmsg.perfdata = perf
	self:set_request(rmsg)
	serialized = msg:serialized()
	result_code, response = core:query(serialized)
	response_message = protobuf.Plugin.QueryResponseMessage.parsefromstring(response)


	found = False
	for i = 0,10 do
		if (self:has_response(uid)) then
			rmsg = self:get_response(uid)
			--#result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%tag)
			result:add_message(rmsg.got_simple_response, 'Testing to recieve simple message using '..tag)
			result:add_message(response_message:size_payload() == 1, 'Verify that we only get one payload response for '..tag)
			pl = response_message:get_payload(1)
			result:assert_equals(pl:get_result(), test.status_to_int(status), 'Verify that status is sent through '..tag)
			if payload_length == 0 then
				result:assert_equals(pl:get_message(), rmsg.message, 'Verify that message is sent through '..tag)
			else
				max_len = payload_length
				if max_len >= length then
					max_len = length - 1
				end
				result:assert_equals(string.len(pl:get_message()), max_len, 'Verify that message length is correct ' .. max_len .. ': ' ..tag)
			end
			--#result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through')
			self:del_response(uid)
			found = true
			break
		else
			core:log('info', string.format('Waiting for %s (%s/%s)', uid,tag,target))
			nscp.sleep(500)
		end
	end
	if (not found) then
		result:add_message(false, string.format('Failed to find message %s using %s', uid, tag))
	end
	
	return result
end

function TestExtScript:test_one(ssl, length, payload_length, status)
	tag = string.format("%s/%d/%s", tostring(ssl), length, status)
	local result = test.TestResult:new{message=string.format('Testing: %s with various targets', tag)}
	for k,t in pairs({'valid', 'test_rp', 'invalid'}) do
		result:add(self:submit_payload(tag, ssl, length, payload_length, tag .. 'src' .. tag, status, tag .. 'msg' .. tag, '', t))
	end
	return result
end

function TestExtScript:do_one_test(ssl, length)
	if ssl == nil then ssl = true end
	length = length or 1024
	
	local conf = nscp.Settings()
	local core = nscp.Core()
	conf:set_int('/settings/NRPE/test_nrpe_server', 'payload length', length)
	conf:set_bool('/settings/NRPE/test_nrpe_server', 'use ssl', ssl)
	conf:set_bool('/settings/NRPE/test_nrpe_server', 'allow arguments', true)
	core:reload('test_nrpe_server')

	conf:set_string('/settings/NRPE/test_nrpe_client/targets/default', 'address', 'nrpe://127.0.0.1:35666')
	conf:set_bool('/settings/NRPE/test_nrpe_client/targets/default', 'use ssl', not ssl)
	conf:set_int('/settings/NRPE/test_nrpe_client/targets/default', 'payload length', length*3)

	conf:set_string('/settings/NRPE/test_nrpe_client/targets/invalid', 'address', 'nrpe://127.0.0.1:25666')
	conf:set_bool('/settings/NRPE/test_nrpe_client/targets/invalid', 'use ssl', not ssl)
	conf:set_int('/settings/NRPE/test_nrpe_client/targets/invalid', 'payload length', length*2)

	conf:set_string('/settings/NRPE/test_nrpe_client/targets/valid', 'address', 'nrpe://127.0.0.1:15666')
	conf:set_bool('/settings/NRPE/test_nrpe_client/targets/valid', 'use ssl', ssl)
	conf:set_int('/settings/NRPE/test_nrpe_client/targets/valid', 'payload length', length)
	core:reload('test_nrpe_client')

	local result = test.TestResult:new{message="Testing "..tostring(ssl)..", "..tostring(length)}
	result:add(self:test_one(ssl, length, 0, 'unknown'))
	result:add(self:test_one(ssl, length, 0, 'ok'))
	result:add(self:test_one(ssl, length, 0, 'warn'))
	result:add(self:test_one(ssl, length, 0, 'crit'))
	result:add(self:test_one(ssl, length, length/2, 'ok'))
	result:add(self:test_one(ssl, length, length, 'ok'))
	result:add(self:test_one(ssl, length, length*2, 'ok'))
	return result
end

function TestExtScript:test_timeout(ssl, server_timeout, client_timeout, length)

	local conf = nscp.Settings()
	local core = nscp.Core()
	conf:set_bool('/settings/NRPE/test_nrpe_server', 'use ssl', ssl)
	conf:set_int('/settings/NRPE/test_nrpe_server', 'timeout', server_timeout)
	conf:set_bool('/settings/NRPE/test_nrpe_server', 'allow arguments', true)
	conf:set_int('/settings/NRPE/test_nrpe_server', 'payload length', length)
	core:reload('test_nrpe_server')

	conf:set_string('/settings/NRPE/test_nrpe_client/targets/default', 'address', 'nrpe://127.0.0.1:15666')
	conf:set_bool('/settings/NRPE/test_nrpe_client/targets/default', 'use ssl', ssl)
	conf:set_int('/settings/NRPE/test_nrpe_client/targets/default', 'timeout', client_timeout)
	conf:set_int('/settings/NRPE/test_nrpe_client/targets/default', 'payload length', length)

	core:reload('test_nrpe_client')

	local result = test.TestResult:new{message="Testing timeouts ssl: "..tostring(ssl)..", server: "..tostring(server_timeout)..", client: "..tostring(client_timeout)}

	local msg = protobuf.Plugin.QueryRequestMessage.new()
	hdr = msg:get_header()
	hdr:set_recipient_id('test')
	host = hdr:add_hosts()
	host:set_address("127.0.0.1:15666")
	host:set_id('test')
	meta = hdr:add_metadata()
	meta:set_key("command")
	meta:set_value('check_py_nrpe_test_s')
	meta = hdr:add_metadata()
	meta:set_key("retry")
	meta:set_value('0')

	uid = string.random(12)
	payload = msg:add_payload()
	payload:set_command('nrpe_forward')
	payload:set_arguments(1, uid)
	rmsg = self:get_request(uid)
	rmsg.status = 'ok'
	rmsg.message = 'Hello: Timeout'
	rmsg.perfdata = ''
	self:set_request(rmsg)
	serialized = msg:serialized()
	result_code, response = core:query(serialized)
	response_message = protobuf.Plugin.QueryResponseMessage.parsefromstring(response)


	found = False
	for i = 0,10 do
		if (self:has_response(uid)) then
			rmsg = self:get_response(uid)
			result:add_message(false, string.format('Testing to recieve message using'))
			self:del_response(uid)
			found = true
			break
		else
			core:log('error', string.format('Timeout waiting for %s', uid))
			--sleep(500)
		end
	end
	if (found) then
		result:add_message(false, string.format('Making sure timeout message was never delivered'))
	end
	
	return result
end

function TestExtScript:run()
	local result = test.TestResult:new{message="NRPE Test Suite"}
	result:add(self:do_one_test(true, 1024))
	result:add(self:do_one_test(false, 1024))
	result:add(self:do_one_test(true, 4096))
	result:add(self:do_one_test(true, 65536))
	result:add(self:do_one_test(true, 1048576))

	result:add(self:test_timeout(false, 30, 1, 1048576000))
	result:add(self:test_timeout(false, 1, 30, 1048576000))
	result:add(self:test_timeout(true, 30, 1, 1048576000))
	result:add(self:test_timeout(true, 1, 30, 1048576000))
	return result
end


instances = { TestNRPE }
test.init_test_manager(instances)

function main(args)
	return test.install_test_manager(instances)
end
