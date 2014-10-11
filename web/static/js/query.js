// Class to represent a row in the seat reservations grid
function CommandEntry(entry) {
	var self = this;
    console.log(entry)
	self.name = entry['name'];
	self.desc = entry['info']['description'];
	self.plugs = entry['info']['plugin'];
	self.command = ko.observable(self.name)
    self.params = []
    entry['parameters']['parameter'].forEach(function(entry) {
        entry.first_line = entry.short_description
        self.params.push(entry)
    })
    self.params = entry['parameters']['parameter']
	self.showDetails = ko.observable(false);
    console.log(self.params)
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}

function parseCommand(cmd) {
	args = cmd.match(/\w+|"(?:\\"|[^"])+"/g);
	str = args[0]
	
	for (var i = 1; i < args.length; i++) {
		var sep = "&"
		if (i == 0)
			sep = ""
		if (i == 1)
			sep = "?"
		var s = args[i]
		as = s.split('=', 2);
		if (as.length > 1) {
			str = str + sep +  encodeURIComponent(as[0]) + "=" + encodeURIComponent(as[1])
		} else {
			str = str + sep +  encodeURIComponent(s)
		}
	}
	return str
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
	self.query = ko.observable();
	self.execCommand = ko.observable();

	self.execute = function(command) {
		cmd_query = parseCommand(command.command())
		console.log(cmd_query)
		$.getJSON("/query/" + cmd_query, function(data) {
			r = data['payload'][0]
			r.resultText = parseNagiosResult(r.result)
			self.result(r)
			console.log(r)
			//data['payload']['inventory'].forEach(function(entry) {
			//	self.commands.push(new CommandEntry(entry['name'], entry['info']['description'], entry['info']['plugin']));
			//});
		})
		
	}
	self.show = function(command) {
	        self.query(command)
		self.result(null)
		$("#result").modal('show');
    }
	self.refresh = function() {
		$.getJSON("/registry/inventory", function(data) {
			if (data['payload'][0] && data['payload'][0]['inventory']) {
				self.commands.removeAll()
				data['payload'][0]['inventory'].forEach(function(entry) {
					self.commands.push(new CommandEntry(entry));
				});
			}
			self.commands.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
		})
	}
	self.refresh()
	
	self.addArgument = function(item, event) {
		self.query().command(self.query().command() + " " + event.val)
		$("#command").focus()
	}
}
ko.applyBindings(new CommandViewModel());