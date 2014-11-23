// Class to represent a row in the seat reservations grid
function CommandEntry(entry) {
	var self = this;
	self.name = entry['name'];
	self.desc = entry['info']['description'];
	self.plugs = entry['info']['plugin'];
	self.command = ko.observable(self.name)
    self.params = []
	if (entry['parameters']['parameter']) {
		entry['parameters']['parameter'].forEach(function(entry) {
			entry.first_line = entry.short_description
			entry.desc = entry.long_description.replace(/\n/g, '<br/>')
			self.params.push(entry)
		})
	}
    self.params = entry['parameters']['parameter']
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	
	self.getArgument = function(key) {
		for (var i = 0; i < self.params.length; i++) {
			if (self.params[i].name == key)
				return self.params[i]
		}
		return null
	}
	self.commandTips = function() {
		return function findMatches(q, cb) {
			var matches, substrRegex;
			var pos = q.lastIndexOf(' ')
			var prefix = ''
			if (pos != -1) {
				prefix = q.substr(0,pos+1)
				q = q.substr(pos+1)
			}
			matches = [];
			substrRegex = new RegExp(q, 'i');
			$.each(self.params, function(i, item) {
				if (substrRegex.test(item.name)) {
					if (item.content_type == "BOOL")
						matches.push({ value: prefix+item.name, key: item.name, tip: item.first_line });
					else
						matches.push({ value: prefix+'"'+item.name+'="', key: item.name, tip: item.first_line });
				}
			});
			cb(matches);
		};
	}
}

function format(state) {
    return "<b>" + state.text + "</b>: " + $(state.element).data('desc');
}

function parseCommand(cmd) {
	args = cmd.match(/[\w=-]+|"(?:\\"|[^"]|[=-])+"/g);
	str = args[0]
	
	for (var i = 1; i < args.length; i++) {
		var sep = "&"
		if (i == 0)
			sep = ""
		if (i == 1)
			sep = "?"
		var s = args[i]
		if (s.substring(0, 1) === '"' && s.substr(s.length-1) === '"')
			s = s.substring(1, s.length-1)
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

	self.nscp_status = ko.observable(global_status);
	self.commands = ko.observableArray([]);
	self.result = ko.observable();
	self.query = ko.observable();
	self.execCommand = ko.observable();

	self.execute = function(command) {
		cmd_query = parseCommand(self.query().command())
		json_get("/query/" + cmd_query, function(data) {
			r = data['payload'][0]
			if (r.perf) {
				for (var i=0;i<r.perf.length;i++) {
					if (r.perf[i].int_value) {
						r.perf[i].int_value.r_value = r.perf[i].int_value.value
						r.perf[i].int_value.r_warning = r.perf[i].int_value.warning
						r.perf[i].int_value.r_critical = r.perf[i].int_value.critical
						r.perf[i].int_value.r_minimum = r.perf[i].int_value.minimum
						r.perf[i].int_value.r_maximum = r.perf[i].int_value.maximum
						r.perf[i].value = r.perf[i].int_value
					} else if (r.perf[i].float_value) {
						r.perf[i].float_value.r_value = Math.round(r.perf[i].float_value.value * 100) / 100
						r.perf[i].float_value.r_warning = Math.round(r.perf[i].float_value.warning * 100) / 100
						r.perf[i].float_value.r_critical = Math.round(r.perf[i].float_value.critical * 100) / 100
						r.perf[i].float_value.r_minimum = Math.round(r.perf[i].float_value.minimum * 100) / 100
						r.perf[i].float_value.r_maximum = Math.round(r.perf[i].float_value.maximum * 100) / 100
						r.perf[i].value = r.perf[i].float_value
					}
				}
			} else {
				r.perf = []
			}
			self.result(r)
		})
		
	}
	self.show = function(command) {
		self.query(command)
		self.result(null)
		$("#result").modal('show');
    }
	self.refresh = function() {
		self.nscp_status().busy('Refreshing', 'Refreshing data...')
		json_get("/registry/inventory", function(data) {
			if (data['payload'][0] && data['payload'][0]['inventory']) {
				self.commands.removeAll()
				data['payload'][0]['inventory'].forEach(function(entry) {
					self.commands.push(new CommandEntry(entry));
				});
			}
			self.commands.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
			self.nscp_status().not_busy();
		})
	}
	self.refresh()
	
	self.addArgument = function(item, event) {
		var e = item.getArgument(event.val)
		if (e == null)
			return
		var type = e['content_type']
		if (type == 'STRING')
			self.query().command(self.query().command() + " " + event.val + '="" ')
		else
			self.query().command(self.query().command() + " " + event.val + " ")
		$("#command").focus()
	}
}
ko.applyBindings(new CommandViewModel());