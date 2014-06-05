function PathNode(parent, path) {
	var self = this;
	self.href = "/settings.html?path=" + parent + "/" + path
	self.title = "Show all keys under: " + parent + "/" + path
	self.name = path
}
	
function make_paths_from_string(path) {
	paths = []
	trail = "";
	path.split('/').forEach(function(entry) {
		if (entry.length > 0) {
			self.paths.push(new PathNode(trail, entry))
			trail = trail + "/" + entry
		}
	});
	return paths
}

function SettingsStatus(elem) {
	var self = this;
	self.has_changed = ko.observable('')
	self.context = ko.observable('')
	self.type = ko.observable('')
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	self.update = function(elem) {
		self.context(elem['context'])
		self.type(elem['type'])
		self.has_changed(elem['has_changed'])
	}
	
}

function PathEntry(entry) {
	var self = this;
	
	self.path = entry['node']['path'];
	self.title = entry['info']['title'];
	self.href = "/settings.html?path=" + self.path
	self.paths = make_paths_from_string(self.path)
	self.desc = entry['info']['description'];
	self.plugs = entry['info']['plugin'];
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}


function LocalKeyEntry(path) {
	var self = this;
	self.value = ko.observable('')
	self.path = path;
	self.key = ''
	self.type = 'string'
}

function KeyEntry(entry) {
	var self = this;
	self.value = ko.observable('')

	if (entry['value'])
		self.value(entry['value']['value'])
	self.path = entry['node']['path'];
	self.key = ''
	if (entry['node']['key'])
		self.key = entry['node']['key'];
	self.paths = make_paths_from_string(self.path)
	self.title = entry['info']['title'];
	self.desc = entry['info']['description'];
	self.plugs = entry['info']['plugin'];
	self.advanced = entry['info']['advanced'];
	self.default_value = ''
	self.type = 'string'
	if (entry['info']['default_value']) {
		self.default_value = entry['info']['default_value']['value'];
		self.type = entry['info']['default_value']['type'];
	}
	if (self.value() == self.default_value)
		self.value('')
	self.old_value = self.value()
	
}

function build_settings_payload(value) {
	console.log(value.key + "=" + value.value())
	payload = {}
	payload['plugin_id'] = 1234
	payload['update'] = {}
	payload['update']['node'] = {}
	payload['update']['node']['path'] = value.path
	payload['update']['node']['key'] = value.key
	payload['update']['value'] = {}
	payload['update']['value']['type'] = 'string'
	payload['update']['value']['value'] = value.value()
	return payload
}

function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(new NSCPStatus());
	path = getUrlVars()['path']
	if (!path)
		path = '/'
	self.currentPath = ko.observableArray(make_paths_from_string(path))
	self.paths = ko.observableArray([]);
	self.keys = ko.observableArray([]);
	self.akeys = ko.observableArray([]);
	self.current = ko.observable();
	self.status = ko.observable(new SettingsStatus());
	self.addNew = ko.observable(new LocalKeyEntry(path));

	self.showAddKey = function(command) {
		$("#addKey").modal('show');
	}
	self.toggleAdvanced = function(command) {
		$("#adkeys").modal($('#myModal').hasClass('in')?'hide':'show');
	}
	self.addNewKey = function(command) {
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [build_settings_payload(self.addNew())];

		$.post("/settings/query.json", JSON.stringify(root), function(data) {
			self.refresh()
		})
	}
	self.save = function(command) {
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [];
		self.keys().forEach(function(entry) {
			if (entry.old_value != entry.value())
				root['payload'].push(build_settings_payload(entry))
		})
		self.akeys().forEach(function(entry) {
			if (entry.old_value != entry.value())
				root['payload'].push(build_settings_payload(entry))
		})
		if (root['payload'].length > 0) {
			$.post("/settings/query.json", JSON.stringify(root), function(data) {
				self.refresh()
			})
		} else {
			self.nscp_status().message("warn", "Settings not saved", "No changes detected");
		}
		
	}
	self.loadStore = function(command) {
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		payload = {}
		payload['plugin_id'] = 1234
		payload['control'] = {}
		payload['control']['command'] = 'LOAD'
		root['payload'] = [ payload ];
		
		$.post("/settings/query.json", JSON.stringify(root), function(data) {
			self.refresh()
		})
	}
	self.saveStore = function(command) {
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		payload = {}
		payload['plugin_id'] = 1234
		payload['control'] = {}
		payload['control']['command'] = 'SAVE'
		
		root['payload'] = [ payload ];
		$.post("/settings/query.json", JSON.stringify(root), function(data) {
			self.refresh()
		})
	}
	self.refresh = function() {
		$.getJSON("/settings/inventory?path=" + path + "&recursive=true&paths=true", function(data) {
			self.paths.removeAll()
			if (data['payload'][0]['inventory']) {
				data['payload'][0]['inventory'].forEach(function(entry) {
					if (path == entry['node']['path']) {
						self.current(new PathEntry(entry))
					} else {
						self.paths.push(new PathEntry(entry));
					}
				});
			}
		})
		$.getJSON("/settings/inventory?path=" + path + "&recursive=false&keys=true", function(data) {
			self.keys.removeAll()
			self.akeys.removeAll()
			if (data['payload'][0]['inventory']) {
				data['payload'][0]['inventory'].forEach(function(entry) {
					key = new KeyEntry(entry)
					if (key.advanced)
						self.akeys.push(key);
					else
						self.keys.push(key);
				});
			}
		})
		self.keys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
		self.akeys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
		$.getJSON("/settings/status", function(data) {
			self.status().update(data['payload'][0]['status'])
		})
	}
	self.set_default_value = function(key) {
		key.value('') // key.default_value
	}
	self.refresh()
}
ko.applyBindings(new CommandViewModel());