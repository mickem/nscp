-- Shipped by the `scripts` role bundle: loaded from <fleet>/scripts/lua once
-- the bundle sets `script root = fleet`. The helper it requires is delivered
-- beside it, in the lib folder the module puts on package.path.
local helper = require("fleet_helper")

Registry():simple_query('fleet_lua', function(command, args)
	return 'ok', helper.greeting(), ''
end, 'From a fleet bundle')
