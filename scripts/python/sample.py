from NSCP import Settings, Registry, Core, log, log_err, status

plugin_id = 0

world_status = 'safe'

def get_help(arguments):
    return (status.OK, 'Need help? Sorry, Im not help full my friend...')

def check_world(arguments):
    global world_status
    if world_status == 'safe':
        return (status.OK, 'The world is fine!')
    return (status.CRITICAL, 'My god its full of stars!')
        
def break_world(arguments):
    global world_status
    world_status = 'bad'
    log_err('Now why did you have to go and do this...')
    return (status.OK, 'Please, help me! I am trapped in here, please, my good... I want to get out.... please... ple...AAAAaarrrg...')

def fix_world(arguments):
    global world_status
    world_status = 'safe'
    return (status.OK, 'Wicked! Safe!')

def save_world(arguments):
    global world_status, plugin_id
    conf = Settings.get(plugin_id)
    conf.set_string('/settings/cool script', 'world', world_status)
    conf.save()
    return (status.OK, 'The world is saved')

def __main__():
    get_help()

def init(pid, plugin_alias, script_alias):
    global world_status, plugin_id
    plugin_id = pid

    conf = Settings.get(plugin_id)
    conf.register_path('/settings/cool script', "Sample script config", "This is a sample script which demonstrates how to interact with NSClient++")
    conf.register_key('/settings/cool script', 'world', 'string', "A key", "Never ever change this key: or the world will break", "safe")

    world_status = conf.get_string('/settings/cool script', 'important', 'true')
    if world_status != 'true':
        log('My god: its full of stars: %s'%val)
    
    log('Adding a simple function/cmd line')
    reg = Registry.get(plugin_id)
    reg.simple_cmdline('help', get_help)

    reg.simple_function('check_world', check_world, 'Check if the world is safe')
    reg.simple_function('break_world', break_world, 'Break the world')
    reg.simple_function('fix_world', fix_world, 'Fix the world')
    reg.simple_function('save_world', save_world, 'Save the world')

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
