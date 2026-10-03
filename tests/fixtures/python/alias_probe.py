# Fixture for tests/pythonscript-api.test.ts: loaded twice, under two script
# aliases, to show what init() receives for each.
from NSCP import Registry, status

def init(plugin_id, plugin_alias, script_alias):
    def probe(args):
        return (status.OK, 'plugin_alias=%s script_alias=%s' % (plugin_alias, script_alias))
    Registry.get(plugin_id).simple_function('alias_probe_' + script_alias, probe, 'What init() was given')
