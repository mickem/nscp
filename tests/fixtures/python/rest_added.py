# Fixture for tests/pythonscript-api.test.ts: uploaded through
# PUT /api/v2/scripts/py, so it is not configured when the agent starts.
from NSCP import Registry, status

def init(plugin_id, plugin_alias, script_alias):
    Registry.get(plugin_id).simple_function('py_rest_added', lambda a: (status.OK, 'rest added'), 'From an uploaded script')
