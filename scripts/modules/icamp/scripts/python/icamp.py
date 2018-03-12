from NSCP import Settings, Registry, Core

def init(plugin_id, plugin_alias, script_alias):

    conf = Settings.get(plugin_id)
    if conf.get_string('/settings/icamp', 'configured', 'false') == 'false':
        conf.set_string('/settings/icamp', 'configured', 'true')
        
        conf.set_string('/modules', 'GraphiteClient', 'enabled')
        conf.set_string('/modules', 'Scheduler', 'enabled')
        conf.set_string('/modules', 'CheckSystem', 'enabled')
        conf.set_string('/modules', 'ElasticClient', 'enabled')
        conf.set_string('/modules', 'CheckEventlog', 'enabled')

        conf.set_string('/settings/graphite/client/targets/default', 'host', '127.0.0.1')

        conf.set_string('/settings/scheduler/schedules/default', 'interval', '5s')
        conf.set_string('/settings/scheduler/schedules/default', 'channel', 'graphite')
        conf.set_string('/settings/scheduler/schedules', 'cpu', 'check_cpu')

        conf.set_string('/settings/elastic/client', 'address', 'http://127.0.0.1:9200/_bulk')
        conf.set_string('/settings/elastic/client', 'events', 'eventlog:errors,system.process:notepad')

        conf.set_string('/settings/eventlog/real-time', 'enabled', 'true')
        conf.set_string('/settings/eventlog/real-time/filters/errors', 'log', 'application')
        conf.set_string('/settings/eventlog/real-time/filters/errors', 'destination', 'events')
        conf.set_string('/settings/eventlog/real-time/filters/errors', 'filter', "level='error'")

        conf.set_string('/settings/system/windows/real-time/process/notepad', 'process', "notepad.exe")
        conf.set_string('/settings/system/windows/real-time/process/notepad', 'filter', "new = 1")
        conf.set_string('/settings/system/windows/real-time/process/notepad', 'destination', "events")
        conf.save()

        core = Core.get(plugin_id)
        core.reload('service')
       