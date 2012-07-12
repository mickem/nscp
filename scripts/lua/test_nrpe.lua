valid_chars = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
	"0","1","2","3","4","5","6","7","8","9","-"}

math.randomseed(os.time())

function random(len) -- args: smallest and largest possible password lengths, inclusive
	pass = {}
	for z = 1,len do
		case = math.random(1,2)
		a = math.random(1,#valid_chars)
		if case == 1 then
			x=string.upper(valid_chars[a])
		elseif case == 2 then
			x=string.lower(valid_chars[a])
		end
	table.insert(pass, x)
	end
	return(table.concat(pass))
end
string.random = random

function status_to_int(status)
	if status == 'ok' then
		return 0
	elseif status == 'warn' then
		return 1
	elseif status == 'crit' then
		return 2
	elseif status == 'unknown' then
		return 3
	else
		print("Unknown status: "..status)
		return 3
	end
end

TestResult = { status = true; children = {} }
function TestResult:new(o)
	o = o or {}
	o["children"] = o["children"] or {}
	if o["status"] == nil then o["status"] = true end
	setmetatable(o, self)
	self.__index = self
	return o

end
function TestResult:add(result)
	if not result.status then self.status = false end
	table.insert(self.children,result)
end
function TestResult:add_message(result, message)
	table.insert(self.children,TestResult:new{status=status, message=message})
end
function TestResult:assert_equals(a, b, message)
	if a==b then
		table.insert(self.children,TestResult:new{status=true, message=message})
	else
		table.insert(self.children,TestResult:new{status=false, message=message..': '..tostring(a)..' != '..tostring(b)})
	end
end


function TestResult:print(indent)
	indent = indent or 0
	pad = string.rep(' ', indent)
	if self.status then
		print(pad .. "[OK ] - " .. self.message)
	else
		print(pad .. "[ERR] - " .. self.message)
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do v:print(indent+2) end
	end
end

function TestResult:print_failed(indent)
	indent = indent or 0
	pad = string.rep(' ', indent)
	if not self.status then
		print(pad .. "[ERR] - " .. self.message)
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do v:print(indent+2) end
	end
end

function TestResult:count()
	local ok = 0
	local err = 0
	if self.status then
		ok = ok + 1
	else
		err = err + 1
	end
	if # self.children > 0 then
		for i,v in ipairs(self.children) do
			local lok, lerr = v:count() 
			ok = ok + lok
			err = err + lerr
		end
	end
	return ok, err
end
	


function TestResult:get_nagios()
	local ok, err = self:count()
	if not self.status then
		return 'crit', tostring(err)..' test cases failed', ''
	else
		return 'ok', tostring(ok)..' test cases succeded', ''
	end
end


TestMessage = {
	uuid = nil,
	source = nil,
	command = nil,
	status = nil,
	message = nil,
	perfdata = nil,
	got_simple_response = false,
	got_response = false
}
	
TestNRPE = {
	requests = {}, 
	responses = {} 
}
function TestNRPE:install(arguments)
	local conf = Settings()
	
	conf:set_string('/modules', 'test_nrpe_server', 'NRPEServer')
	conf:set_string('/modules', 'test_nrpe_client', 'NRPEClient')
	conf:set_string('/modules', 'luatest', 'LuaScript')

	conf:set_string('/settings/luatest/scripts', 'test_nrpe', 'test_nrpe.lua')
	
	conf:set_string('/settings/NRPE/test_nrpe_server', 'port', '15666')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'inbox', 'nrpe_test_inbox')
	conf:set_string('/settings/NRPE/test_nrpe_server', 'encryption', '1')

	conf:set_string('/settings/NRPE/test_nrpe_client/targets', 'nrpe_test_local', 'nrpe://127.0.0.1:15666')
	conf:set_string('/settings/NRPE/test_nrpe_client', 'channel', 'nrpe_test_outbox')
	--conf:save()
end

function TestNRPE:setup()
	local reg = Registry(plugin_id)
	reg:simple_query('check_py_nrpe_test_s', self.simple_handler, 'TODO')
	reg:query('check_py_nrpe_test', self.handler, 'TODO')
end

function TestNRPE:uninstall()
end

function TestNRPE:help()
end

function TestNRPE:init(plugin_id)
end

function TestNRPE:shutdown()
end

function TestNRPE:has_response(id)
	return self.responses[id]
end

function TestNRPE:get_response(id)
	msg = self.requests[id]
	if msg == nil then
		msg = TestMessage
		msg.uuid=id
		self.responses[id] = msg
		return msg
	end
	return msg
end

function TestNRPE:set_response(msg)
	self.responses[msg.uuid] = msg
end

function TestNRPE:del_response(id)
	self.responses[id] = nil
end
		
function TestNRPE:get_request(id)
	msg = self.requests[id]
	if msg == nil then
		msg = TestMessage
		msg.uuid=id
		self.requests[id] = msg
		return msg
	end
	return msg
end

function TestNRPE:set_request(msg)
	self.requests[msg.uuid] = msg
end

function TestNRPE:del_request(id)
	self.requests[id] = nil
end

function TestNRPE.simple_handler(command, args)
	msg = instance:get_response(args[0])
	msg.got_simple_response = true
	instance:set_response(msg)
	rmsg = instance:get_request(args[0])
	return rmsg.status, rmsg.message, rmsg.perfdata
end

function TestNRPE.handler(req)
	msg = instance:get_response(args[0])
	msg.got_response = true
	instance:set_response(msg)
	rmsg = instance:get_request(args[0])
end

function TestNRPE:submit_payload(tag, ssl, length, source, status, message, perf, target)
	local core = Core()
	local result = TestResult:new{message='Testing NRPE: '..tag..' for '..target}
	
	local msg = protobuf.Plugin.QueryRequestMessage.new()
	hdr = msg:get_header()
	hdr:set_version(1)
	hdr:set_recipient_id(target)
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
		enc:set_value('5')
	end

	uid = string.random(12)
	payload = msg:add_payload()
	payload:set_command('check_py_nrpe_test_s')
	payload:set_arguments(1, uid)
	rmsg = self:get_request(uid)
	rmsg.status = status
	rmsg.message = message
	rmsg.perfdata = perf
	self:set_request(rmsg)
	serialized = msg:serialized()
	result_code, response = core:query('nrpe_forward', serialized)
	response_message = protobuf.Plugin.QueryResponseMessage.parsefromstring(response)
	--print(response_message:get_payload(1):get_message())


	found = False
	for i = 0,10 do
		if (self:has_response(uid)) then
			rmsg = self:get_response(uid)
			--#result.add_message(rmsg.got_response, 'Testing to recieve message using %s'%tag)
			result:add_message(rmsg.got_simple_response, 'Testing to recieve simple message using '..tag)
			result:add_message(response_message:size_payload() == 1, 'Verify that we only get one payload response for '..tag)
			pl = response_message:get_payload(1)
			result:assert_equals(pl:get_result(), status_to_int(status), 'Verify that status is sent through '..tag)
			result:assert_equals(pl:get_message(), rmsg.message, 'Verify that message is sent through '..tag)
			--#result.assert_equals(rmsg.perfdata, perf, 'Verify that performance data is sent through')
			self:del_response(uid)
			found = True
			break
		else
			log(string.format('Waiting for %s (%s/%s)', uid,tag,target))
			--sleep(500)
		end
	end
	if (not found) then
		result:add_message(false, string.format('Testing to recieve message using %s', tag))
	end
	
	return result
end

function TestNRPE:test_one(ssl, length, status)
	tag = string.format("%s/%d/%s", tostring(ssl), length, status)
	result = TestResult:new{message=string.format('Testing: %s with various targets', tag)}
	for k,t in pairs({'valid', 'test_rp', 'invalid'}) do
		result:add(self:submit_payload(tag, ssl, length, tag .. 'src' .. tag, status, tag .. 'msg' .. tag, '', t))
	end
	return result
end

function TestNRPE:do_one_test(ssl, length)
	if ssl == nil then ssl = true end
	length = length or 1024
	
	local conf = Settings()
	local core = Core()
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

	result = TestResult:new{message="Testing "..tostring(ssl)..", "..tostring(length)}
	result:add(self:test_one(ssl, length, 'unknown'))
	result:add(self:test_one(ssl, length, 'ok'))
	result:add(self:test_one(ssl, length, 'warn'))
	result:add(self:test_one(ssl, length, 'crit'))
	return result
end


function TestNRPE:run()
	local result = TestResult:new{message="NRPE Test Suite"}
	result:add(self:do_one_test(true, 1024))
	result:add(self:do_one_test(false, 1024))
	result:add(self:do_one_test(true, 4096))
	result:add(self:do_one_test(true, 65536))
	result:add(self:do_one_test(true, 1048576))
	return result
end


function lua_unittest_handler(command, args)
	result = instance:setup()
	result = instance:run()
	print "--"
	result:print()
	print "--"
	result:print_failed()
	print "--"
	return result:get_nagios()
end
function install_test_manager()
	instance:install({})
	local reg = Registry(plugin_id)
	reg:simple_query('lua_unittest', lua_unittest_handler, 'TODO')
end

instance = TestNRPE
install_test_manager()
