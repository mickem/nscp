function install()
    -- Used to install this script
    local conf = nscp.Settings()
    conf:set_string('/modules', 'CheckSystem', 'enabled')
    conf:set_string('/modules', 'CheckHelpers', 'enabled')
    conf:set_string('/modules', 'LUAScript', 'enabled')
    conf:set_string('/settings/lua/scripts', 'check_cpu_ex', 'check_cpu_ex')
    conf:save()
end
 
function setup()
    -- register our function
    local reg = nscp.Registry()
    reg:simple_query('check_cpu_ex', check_cpu_ex, 'Check CPU version which returns top consumers')
end
 
function check_cpu_ex(command, arguments)
    local core = nscp.Core()
    cpu_result, cpu_message, cpu_perf = core:simple_query('check_cpu', arguments)
    if cpu_result == 'UNKNOWN' then
        core:log('error', string.format('Invalid return from check_cpu: %s', cpu_result))
        return cpu_result, cpu_message, cpu_perf
    end
    -- Status is good, lets execute check_process and filter_perf.
    proc_result, proc_message, proc_perf = core:simple_query('filter_perf', {'command=check_process', 'sort=normal', 'limit=5', 'arguments', 'delta=true', 'warn=time>0', 'filter=time>0'})
    return cpu_result, 'Top preformers: ' .. proc_perf, cpu_perf
end
 
setup()
 
function main(args)
    cmd = args[0] or ''
    if cmd == 'install' then
        install()
        return 'ok', 'Script installed'
    else
        return 'error', 'Usage: .. install'
    end
end