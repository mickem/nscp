from NSCP import Settings, Registry, Core, log, log_error, status
import urllib
import os
import shutil
import argparse

path = None

class URLopenerWErr(urllib.FancyURLopener):
    def http_error_default(self, url, fp, errcode, errmsg, headers):
        raise Exception("Failed to fetch %s %s:%s"%(url, errcode, errmsg))

fetcher = URLopenerWErr()

def get_module_name(url):
    return url[url.rfind("/")+1:]

def get_target_file(url):
    return os.path.join(path, get_module_name(url))

def download_module(url, target_file):
    try:
        (filename, headers) = fetcher.retrieve(url)
        shutil.copyfile(filename, target_file)
        log("Fetched: %s"%target_file)
        return True
    except Exception as e:
        log("Failed to fetch module: %s"%e)
    return False

def update_modules(module = None):
    ret = False
    conf = Settings.get(plugin_id)
    for mod in conf.get_section('/settings/remote-modules/modules'):
        if module and not mod == module:
            continue
        url = conf.get_string('/settings/remote-modules/modules', mod, '')
        log("Fetching module %s as %s"%(mod, url))
        file = get_target_file(url)
        if not download_module(url, file):
            log_error("Failed to fetch module: %s"%mod)
            continue
        ret = True
    return ret
    
def enable_modules(module = None):
    ret = False
    conf = Settings.get(plugin_id)
    core = Core.get(plugin_id)
    for mod in conf.get_section('/settings/remote-modules/modules'):
        if module and not mod == module:
            continue
        url = conf.get_string('/settings/remote-modules/modules', mod, '')
        file = get_module_name(url)
        log("Loading module %s"%file)
        core.load_module(file, "")
    return ret

def update(arguments = []):
    try:
        parser = argparse.ArgumentParser(description='Update modules', prog="remote_module_update")
        parser.add_argument('--all', action='store_true',
                            help='Fetch all modules')
        parser.add_argument('--module', action='store',
                            help='Fetch a single module (given its name)')
        args = parser.parse_args(arguments)
    except SystemExit as e:
        return (status.UNKNOWN, "%s"%e)
    ret = False
    if args.all:
        ret = update_modules()
    elif args.module:
        ret = update_modules(args.module)
    else:
        return (status.UNKNOWN, "Invalid command line")
    if ret:
        return (status.OK, "Modules update successfully")
    return (status.CRITICAL, "Failed to update modules")

def run_tests(arguments = []):
    result = get_test_manager().run(arguments)
    return result.return_nagios(get_test_manager().show_all)

def init(pid, plugin_alias, script_alias):
    global plugin_id, icinga_url, icinga_auth, path
    plugin_id = pid

    core = Core.get(plugin_id)
    path = core.expand_path("${module-path}")
    
    conf = Settings.get(plugin_id)
    conf.register_path('/settings/remote-modules', "Remote module", "Keys for the remote-modules module which handles downloading remote modules on demand")
    conf.register_path('/settings/remote-modules/modules', "Remote modules", "A list of remote modules to fetch the key is not used.")
    
    reg = Registry.get(plugin_id)
    reg.simple_function('remote_module_update', update, 'Update all or one remote module')
    
    for mod in conf.get_section('/settings/remote-modules/modules'):
        url = conf.get_string('/settings/remote-modules/modules', mod, '')
        log("Adding module %s as %s"%(mod, url))
        conf.register_key('/settings/remote-modules/modules', mod, 'string', "A remote module to fetch", "A remote module to fetch", "")
    if not update_modules():
        log_error("Failed to update modules")
    else:
        enable_modules()
        

def shutdown():
    None
