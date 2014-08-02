// Class to represent a row in the seat reservations grid
function CommandEntry(name, desc, plugs) {
	var self = this;
	self.name = name;
	self.desc = desc;
	self.plugs = plugs;
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}

function parseNagiosResult(id) {
	if (id == 0)
		return "OK"
	if (id == 1)
		return "WARNING"
	if (id == 2)
		return "CRITCAL"
	if (id == 3)
		return "UNKNOWN"
	return "INVALID(" + id + ")"
}
function parseNagiosResultCSS(id) {
	console.log(id)
	if (id == 0)
		return "btn-success"
	if (id == 1)
		return "btn-warning"
	if (id == 2)
		return "btn-danger"
	return "btn-info"
}

// Overall viewmodel for this screen, along with initial state
function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(new NSCPStatus());
	self.commands = ko.observableArray([]);
	self.result = ko.observable();

	self.execute = function(command) {
		$("#result").modal('show');
		
		$.getJSON("/query/" + command.name, function(data) {
			data['payload'].resultText = parseNagiosResult(data['payload'].result)
			self.result(data['payload'])
			//data['payload']['inventory'].forEach(function(entry) {
			//	self.commands.push(new CommandEntry(entry['name'], entry['info']['description'], entry['info']['plugin']));
			//});
		})
		
	}
	self.load = function() {
		$.getJSON("/registry/inventory", function(data) {
			console.log(data)
			if (data['payload'][0] && data['payload'][0]['inventory']) {
				self.commands.removeAll()
				data['payload'][0]['inventory'].forEach(function(entry) {
					self.commands.push(new CommandEntry(entry['name'], entry['info']['description'], entry['info']['plugin']));
				});
			}
			self.commands.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
		})
	}
	self.load()
}
ko.applyBindings(new CommandViewModel());