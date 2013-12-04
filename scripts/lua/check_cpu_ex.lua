--
-- Dont really need to make this an object but...
function install()
	-- Not currently used but could be used to install the script
	local conf = nscp.Settings()
	conf:set_string('/modules', 'CheckSystem', 'enabled')
	conf:set_string('/modules', 'CheckHelpers', 'enabled')
	conf:set_string('/settings/lua/scripts', 'check_cpu_ex', 'check_cpu_ex')
	conf:save()
end

function setup()
	-- register our function
	local reg = nscp.Registry()
	reg:query('check_cpu_ex', check_cpu_ex, 'Check CPU version which returns top consumers')
end

function check_cpu_ex(command, request_payload, request_message)
	local core = nscp.Core()
	req_msg = protobuf.Plugin.QueryRequestMessage.Request.parsefromstring(request_payload)

	-- Create check_cpu query
	local msg = protobuf.Plugin.QueryRequestMessage.new()
	hdr = msg:get_header()
	hdr:set_version(1)
	payload = msg:add_payload()
	payload:set_command('check_cpu')
	-- Copy our arguments
	for i = 1, req_msg:size_arguments(), 1 do
		payload:set_arguments(i, req_msg:get_arguments(i))
	end
	-- Execute query
	result_code, response = core:query(msg:serialized())
	if result_code ~= 'ok' then
		core:log('error', string.format('Invalid return from check_cpu: %s', result_code))
		return result_code
	end
	-- Parse and validate result
	response_message = protobuf.Plugin.QueryResponseMessage.parsefromstring(response)
	if response_message:size_payload() == 0 then
		core:log('error', 'Nothing returned from check_cpu')
		return result_code
	end
	-- Extract first payload (we ignore any additional payload)
	pl = response_message:get_payload(1)
	if pl:get_result() == 0 or pl:get_result() == 1 or pl:get_result() == 2 then
		-- Status is good, lets extract the performance data
		result, message, perf = core:simple_query('filter_perf', {'command=check_process', 'sort=normal', 'limit=5', 'arguments', 'delta=true', 'warn=user>0', 'filter=user>1'})
		pl:set_message('Top preformers: ' .. perf)
		return pl:serialized()
	end
	-- Status is unknown or unexpected log message and return
	core:log('error', string.format('Invalid code from check_process: %s', pl:get_result()))
	return ''
end

setup()
