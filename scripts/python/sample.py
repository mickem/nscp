from NSCP import Settings, Registry, Core, log, log_err, status

plugin_id = 0

world_status = 'safe'
show_metrics = False
world_count = 0
def get_help(arguments):
    if arguments:
        log("Wicked: we got some args")
    return (status.OK, 'Need help? Sorry, Im not help full my friend...')

def check_world(arguments):
    global world_status, world_count
    world_count = world_count + 1
    if world_status == 'safe':
        return (status.OK, 'The world is fine!')
    return (status.CRITICAL, 'My god its full of stars: %s'%world_status)
        
def break_world(arguments):
    global world_status, world_count
    world_count = world_count + 1
    world_status = 'bad'
    log_err('Now why did you have to go and do this...')
    return (status.OK, 'Please, help me! I am trapped in here, please, my good... I want to get out.... please... ple...AAAAaarrrg...')

def fix_world(arguments):
    global world_status, world_count
    world_count = world_count + 1
    world_status = 'safe'
    return (status.OK, 'Wicked! Safe!')

def save_world(arguments):
    global world_status, plugin_id
    conf = Settings.get(plugin_id)
    conf.set_string('/settings/cool script', 'world', world_status)
    conf.save()
    return (status.OK, 'The world is saved: %s'%world_status)

def fun_show_metrics(arguments):
    global show_metrics
    if len(arguments) > 0:
        if arguments[0] == "true":
            show_metrics = True
            return (status.OK, 'Metrics displayed enabled')
        else:
            show_metrics = False
            return (status.OK, 'Metrics displayed disabled')
    return (status.UNKNOWN, 'Usage: show_metrics <true|false>')

def __main__(args):
    get_help(args)

def submit_metrics(list, request):
    global show_metrics
    if show_metrics:
        for k,v in list.iteritems():
            log("Got metrics: %s = %s"%(k,v))

def fetch_metrics():
    global world_status, world_count
    return { "number.of.times": world_count, "world": world_status}

def init(pid, plugin_alias, script_alias):
    global world_status, plugin_id
    plugin_id = pid

    conf = Settings.get(plugin_id)
    conf.register_path('/settings/cool script', "Sample script config", "This is a sample script which demonstrates how to interact with NSClient++")
    conf.register_key('/settings/cool script', 'world', 'string', "A key", "Never ever change this key: or the world will break", "safe")

    world_status = conf.get_string('/settings/cool script', 'world', 'true')
    if world_status != 'safe':
        log('My god: its full of stars: %s'%world_status)
    
    log('Adding a simple function/cmd line')
    reg = Registry.get(plugin_id)
    reg.simple_cmdline('help', get_help)

    reg.simple_function('check_world', check_world, 'Check if the world is safe')
    reg.simple_function('break_world', break_world, 'Break the world')
    reg.simple_function('fix_world', fix_world, 'Fix the world')
    reg.simple_function('save_world', save_world, 'Save the world')
    
    reg.simple_function('show_metrics', fun_show_metrics, 'Enable displaying metrics or not')

    reg.submit_metrics(submit_metrics)
    reg.fetch_metrics(fetch_metrics)

    #core.simple_submit('%stest'%prefix, 'test.py', status.WARNING, 'hello', '')
    #core.simple_submit('test', 'test.py', status.WARNING, 'hello', '')
    
    #(ret, list) = core.simple_exec('%stest'%prefix, ['a', 'b', 'c'])
    #for l in list:
    #	log('-- %s --'%l)

    #log('Testing to register settings keys')
    #conf.register_path('hello', 'PYTHON SETTINGS', 'This is stuff for python')
    #conf.register_key('hello', 'python', 'int', 'KEY', 'This is a key', '42')

    #log('Testing to get key (nonexistant): %d' % conf.get_int('hello', 'python', -1))
    #conf.set_int('hello', 'python', 4)
    #log('Testing to get it (after setting it): %d' % conf.get_int('hello', 'python', -1))

    #log('Saving configuration...')
    #conf.save()

def shutdown():
    log('Unloading script...')
