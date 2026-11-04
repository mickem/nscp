# Shipped by the `scripts` role bundle: loaded from <fleet>/scripts/python once
# the bundle sets `script root = fleet`. The helper it imports is delivered
# beside it, in the lib folder the module puts on sys.path - which is what the
# check proves: a bundle can carry a script and the code it depends on.
from NSCP import Registry, status

import fleet_helper


def init(plugin_id, plugin_alias, script_alias):
    Registry.get(plugin_id).simple_function(
        "fleet_py", lambda args: (status.OK, fleet_helper.greeting()), "From a fleet bundle"
    )
