

function LocalKeyEntry(path) {
	var self = this;
	self.value = ko.observable('')
	self.path = path;
	self.key = ''
	self.type = 'string'
}

var refresh_count = 0;
function done_refresh(self, count) {
	if (!count)
		count = 1
	console.log("done: " + refresh_count + "-" + count)
	refresh_count-=count
	if (refresh_count < 0)
		refresh_count = 0
	if (refresh_count==0)
		self.nscp_status().not_busy()
}
function init_refresh(self) {
	self.nscp_status().busy('Refreshing', 'Refreshing data...')
	refresh_count += 2;
}

function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(new NSCPStatus());
	self.current_paths = ko.observableArray([]);
	self.paths = ko.observableArray([]);
	self.keys = ko.observableArray([]);
	self.akeys = ko.observableArray([]);
	self.current = ko.observable();
	self.path = ko.observable('/');
	self.addNew = ko.observable(new KeyEntry({ 'node': {'path': self.path()} }));
	self.currentPath = ko.observableArray(make_paths_from_string(self.path()))
	self.path_map = {}

	self.showAddKey = function(command) {
		$("#addKey").modal('show');
	}
	self.toggleAdvanced = function(command) {
		$("#adkeys").modal($('#myModal').hasClass('in')?'hide':'show');
	}
	self.addNewKey = function(command) {
		init_refresh(self);
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [self.addNew().build_payload()];

		$.post("/settings/query.json", JSON.stringify(root), function(data) {
			self.refresh()
			done_refresh(self, 2);
		})
	}
	self.save = function(command) {
		init_refresh(self);
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [];
		self.keys().forEach(function(entry) {
			if (entry.old_value != entry.value())
				root['payload'].push(entry.build_payload())
		})
		self.akeys().forEach(function(entry) {
			if (entry.old_value != entry.value())
				root['payload'].push(entry.build_payload())
		})
		if (root['payload'].length > 0) {
			$.post("/settings/query.json", JSON.stringify(root), function(data) {
				self.refresh()
				done_refresh(self, 2);
			})
		} else {
			self.nscp_status().message("warn", "Settings not saved", "No changes detected");
			done_refresh(self, 2);
		}
	}
	self.loadStore = function(command) {
		init_refresh(self);
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
			done_refresh(self, 2);
		})
	}
	self.saveStore = function(command) {
		init_refresh(self);
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
			done_refresh(self, 2);
		})
	}
	self.update_current_paths = function() {
		self.current_paths.removeAll()
		ko.utils.arrayForEach(self.paths(), function(p) {
			if (p.path.indexOf(path) == 0 && p.path != path )  {
				self.current_paths.push(p)
			}
		});
	}
	self.refresh = function() {
		init_refresh(self);
		path = self.path()
		console.log("paths: " + self.paths())
		if (self.paths().length == 0) {
			$.getJSON("/settings/inventory?path=/&recursive=true&paths=true", function(data) {
				if (data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						p = new PathEntry(entry)
						self.paths.push(p);
						self.path_map[p.path] = p
					});
				}
				self.update_current_paths();
				done_refresh(self);
			}).error(function(xhr, error, status) {
				self.nscp_status().not_busy()
				self.nscp_status().set_error(xhr.responseText)
			})
		} else {
			self.update_current_paths();
			done_refresh(self);
		}
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
			self.keys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
			self.akeys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
			done_refresh(self);
		}).error(function(xhr, error, status) {
			self.nscp_status().not_busy()
			self.nscp_status().set_error(xhr.responseText)
		})
	}
	self.set_default_value = function(key) {
		key.value('')
	}
	self.change_path = function(path) {
		p = self.path_map[path]
		if (!p)
			p = new LocalKeyEntry(path)
		self.current(p)
		self.addNew(new KeyEntry({ 'node': {'path': path} }))
		self.path(self.current().path)
		self.currentPath(make_paths_from_string(self.path()))
		self.refresh()
	}
	self.set_root_path = function(command) {
		self.change_path('/')
	}
	self.set_current_path = function(command) {
		self.change_path(command.path)
	}
	self.refresh()
}
ko.applyBindings(new CommandViewModel());

