from NSCP import Core, Registry, log, log_error
plugin_id = -1

def __main__(args):
    global plugin_id
    # List all namespaces recursivly
    core = Core.get(plugin_id)
    (ret, ns_msgs) = core.simple_exec('CheckWMI', 'wmi', ['--list-all-ns'])
    if len(ns_msgs) == 0:
        log_error("Failed to execute WMI command is CheckWMI enabled?")
    else:
        for ns in ns_msgs[0].splitlines():
            # List all classes in each namespace
            (ret, cls_msgs) = core.simple_exec('any', 'wmi', ['--list-classes', '--simple', '--namespace', ns])
            for cls in cls_msgs[0].splitlines():
                log( '%s : %s'%(ns, cls))
   

def init(pid, plugin_alias, script_alias):
    global plugin_id
    plugin_id = pid
