from NSCP import Settings, Registry, Core, log
import importlib
import requests
import json
import socket

plugin_id = 0
icinga_header = {'Accept': 'application/json'}
icinga_url = 'https://192.168.0.1:5665'
icinga_auth = None
server_name = ''

def upsert(path, data):
    r = requests.post(icinga_url+path, data=data, verify=False, headers=icinga_header, auth=icinga_auth)
    if r.status_code == 404:
        r = requests.put(icinga_url+path, data=data, verify=False, headers=icinga_header, auth=icinga_auth)
    if r.status_code == 500:
        log("Failed to upsert: %s: %s"%(path, r.text))
    return r

def post(path, data):
    r = requests.post(icinga_url+path, data=data, verify=False, headers=icinga_header, auth=icinga_auth)
    if r.status_code == 500:
        log("Failed to upsert: %s: %s"%(path, r.text))
    return r

def remove(path):
    r = requests.delete(icinga_url+path, verify=False, headers=icinga_header, auth=icinga_auth)
    if r.status_code == 500:
        log("Failed to delete: %s: %s"%(path, r.text))
    return r

def add_host(server_name, address, os_version):
    payload = {
        "attrs": { 
            "address": address, 
            "check_command": "hostalive",
            "vars.os": "Windows",
            "vars.version": os_version
        }
    }
    r = upsert('/v1/objects/hosts/%s'%server_name, json.dumps(payload))
    log("Host for %s created: %d: %s"%(address, r.status_code, r.text))

def add_nrpe_service(server_name, service_name, command, description, args = []):
    payload = {
        "display_name": description,
        "host_name": server_name,
        "attrs": { 
            "check_command": "nrpe",
            "check_interval": "60",
            "retry_interval": "15",
            "vars.nrpe_command": command,
            "vars.nrpe_arguments": args
        }
    }
    r = upsert('/v1/objects/services/%s!%s'%(server_name, service_name), json.dumps(payload))
    log("Service created: %s!%s %d: %s"%(server_name, service_name, r.status_code, r.text))


def install_module(module):
    m = None
    try:
        m = importlib.import_module('modules.%s'%module)
    except Exception,e:
        log("Failed to load %s: %s"%(module, e))
        return
    try:
        log('Detected %s: enabling %s'%(m.desc(), module))
        m.set_plugin_id(plugin_id)
        m.install()
        for s in m.get_services():
            add_nrpe_service(server_name, s['name'], s['command'], s['desc'], s['args'])
    except Exception,e:
        log("Failed to install %s: %s"%(module, e))
    log("Module %s installed successfully."%module)

def on_event(event, data):
    if 'exe' in data and data['exe'] == 'notepad.exe':
        install_module('notepad')
    if 'exe' in data and data['exe'] == 'nscp.exe':
        install_module('nscp')

nics = []
def found_nic(name, alias):
    if not alias in nics:
        nics.append(alias)
        add_nrpe_service(server_name, 'check_nic_%d'%len(nics), 'check_network', alias, ["filter=name='%s'"%name])

def submit_metrics(list, request):
    for k,v in list.iteritems():
        if k.startswith('system.network') and k.endswith('NetConnectionID'):
            found_nic(k[15:-16], v)
        
def icinga_passive(channel, source, command, code, message, perf):
    payload = {
        "exit_status": int(code),
        "plugin_output": message
#        "performance_data": perf
    }
    r = post('/v1/actions/process-check-result?service=%s!%s'%(server_name, command), json.dumps(payload))

def init(pid, plugin_alias, script_alias):
    global server_name, plugin_id, icinga_url, icinga_auth
    plugin_id = pid
    server_name = socket.gethostname()

    reg = Registry.get(plugin_id)
    core = Core.get(plugin_id)
    reg.event('name', on_event)
    reg.submit_metrics(submit_metrics)
    
    conf = Settings.get(plugin_id)
    conf.register_key('/settings/icinga', 'url', 'string', "The URL of the icinga API port", "The icinga base for the api: https://icinga.com:5665", "")
    conf.register_key('/settings/icinga', 'user', 'string', "The user id of the icinga API", "The icinga API user: root", "")
    conf.register_key('/settings/icinga', 'password', 'string', "The user id of the icinga API", "The icinga API password: hopefully not icinga", "")
    
    icinga_url = conf.get_string('/settings/icinga', 'url', '')
    usr = conf.get_string('/settings/icinga', 'user', '')
    pwd = conf.get_string('/settings/icinga', 'password', '')
    icinga_auth = requests.auth.HTTPBasicAuth(usr, pwd)
    
    (res, os_version, perf) = core.simple_query('check_os_version', ["top-syntax=${list}"])
    add_host(server_name, socket.gethostbyname(server_name), os_version)
    add_nrpe_service(server_name, 'check_cpu', 'check_cpu', 'CPU Load')
    add_nrpe_service(server_name, 'check_memory', 'check_memory', 'CPU Load')

    for k in ['check_cpu', 'check_memory']:
        conf.set_string('/settings/scheduler/schedules/%s'%k, 'command', k)
        conf.set_string('/settings/scheduler/schedules/%s'%k, 'channel', 'icinga_passive')
        conf.set_string('/settings/scheduler/schedules/%s'%k, 'interval', '30s')
    core.load_module('Scheduler', '')

    reg.simple_subscription('icinga_passive', icinga_passive)


def shutdown():
    global server_name
    log('Removing host: %s...'%server_name)
    remove('/v1/objects/hosts/%s?cascade=1'%server_name)

