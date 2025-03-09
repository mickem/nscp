
function client_process(packet)
	cnt = packet:size_section()
	nscp.print('Got packets: ' .. cnt)
	for i = 1, cnt do
		s = packet:get_section(i)
		ln = s:size_line()
		sz = s:get_title()
		nscp.print(' + ' .. sz)
		for j = 1, s:size_line() do
			ln = s:get_line(j)
			nscp.print('    + ' .. ln:get_line())
		end
	end
end

function server_process(packet)
	s = section.new()
	s:set_title("check_mk")
	s:add_line("Version: 0.0.1")
	s:add_line("Agent: nsclient++")
	l = line.new()
	l:add_item("AgentOS:")
	l:add_item("Windows")
	s:add_line(l)
	packet:add_section(s)
end

reg = mk.new()
reg:client_callback(client_process)
reg:server_callback(server_process)
reg = nil