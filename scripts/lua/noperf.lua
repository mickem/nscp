function no_perf(command, args)
	local core = nscp.Core()
	nscp.print ('Uhmmn, 111')
	code, msg, perf = core:simple_query('check_uptime', args)
	nscp.print ('Uhmmn, 22')
	return code, msg, ''
end
local reg = nscp.Registry()
reg:simple_query('check_uptime_no_perf', no_perf, 'Wrapped check-uptime which does not yield performance data')
