
function process(packet)
	cnt = packet:size_section()
	nscp.print('Got packets: ' .. cnt)
	for i = 1, cnt do
		s = packet:get_section(i)
		ln = s:size_line()
		sz = s:get_title()
		nscp.print('  + ' .. ln .. ': ' .. sz)
		for j = 1, s:size_line() do
			ln = s:get_line(j)
			nscp.print('    + ' .. ln:get_line())
		end
	end
end


reg = nscp.check_mk()
reg:client_callback(process)
nscp.print('loaded check_mk processor')