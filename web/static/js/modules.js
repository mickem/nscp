var root;
function Module(entry) {
	var self = this;
	self.name = ko.observable()
	self.title = ko.observable()
	self.desc = ko.observable()
	self.is_loaded = ko.observable(false)
	self.showDetails = ko.observable(false);
	self.is_enabled = ko.observable(false);
	self.keys = ko.observableArray([]);
	self.load = function(entry) {
		self.name(entry['name'])
		self.title(entry['info']['title'])
		self.desc(entry['info']['description'])
		entry['info']['metadata'].forEach(function(entry) {
			if (entry.key == "loaded")
				self.is_loaded(entry.value=="true")
		})
	}
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	
	self.update = function(entry) {
		old = self.is_loaded()
		entry['info']['metadata'].forEach(function(entry) {
			if (entry.key == "loaded")
				self.is_loaded(entry.value=="true")
		})
		if (old != self.is_loaded())
			self.refresh_settings()
	}
	self.update_enabled = function(status) {
		if (status == "1" || status == "enabled")
			self.is_enabled(true)
		else
			self.is_enabled(false)
	}
	self.refresh_settings = function() {
		if (self.is_loaded()) {
			if (self.keys.length == 0) {
				//root.nscp_status().busy('Loading', 'Refresing ' + self.name() + '...')
				$.getJSON("/settings/inventory?path=/&recursive=true&keys=true&module=" + self.name(), function(data) {
					keys = []
					if (data['payload'][0]['inventory']) {
						data['payload'][0]['inventory'].forEach(function(entry) {
							if (entry['info']['plugin'] && entry['info']['plugin'].indexOf(self.name()) != -1) {
								keys.push(new KeyEntry(entry));
							}
						});
					}
					result = groupBy(keys, function(item) { return [item.path]; })
					self.keys(result);
					//root.nscp_status().not_busy()
				});
			}
		} else {
			self.keys.removeAll();
		}
	}
	if (entry)
		self.load(entry)
}

function build_settings_payload(value) {
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
	self.modules = ko.observableArray([]);
	self.currentName = ko.observable('')
	self.module = ko.observable(new Module())
	self.paths = ko.observableArray([]);

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
	self.refresh_settings = function() {
		self.nscp_status().busy('Loading', 'Refresing module list...')
		$.getJSON("/settings/inventory?path=/modules&recursive=false&keys=true", function(data) {
			if (data['payload'] && data['payload'][0]['inventory']) {
				data['payload'][0]['inventory'].forEach(function(entry) {
					name = entry['node']['key']
					var match = ko.utils.arrayFirst(self.modules(), function(item) {
						return item.name() == name;
					});
					if (match) {
						match.update_enabled(settings_get_value(entry['value']))
					}
				});
			}
			self.nscp_status().not_busy()
		})
	}
	self.refresh_modules = function() {
		self.nscp_status().busy('Loading', 'Refresing module list...')
		$.getJSON("/registry/inventory/modules", function(data) {
			added_item = false;
			if (data['payload'][0]['inventory']) {
				data['payload'][0]['inventory'].forEach(function(entry) {
					name = entry['name']
					var match = ko.utils.arrayFirst(self.modules(), function(item) {
						return item.name() == name;
					});
				
					if (!match) {
						self.modules.push(new Module(entry));
						added_item = true;
					} else {
						match.update(entry)
					}
				});
			}
			if (added_item)
				self.modules.sort(function(left, right) { return left.name() == right.name() ? 0 : (left.name() < right.name() ? -1 : 1) })
			self.refresh_settings()
		})
	}
	self.refresh = function() {
		self.refresh_modules()
	}
	self.load = function(command) {
		self.nscp_status().busy('Loading', 'Loading ' + command.name())
		$.getJSON("/registry/control/module/load?name="+command.name(), function(data) {
			self.refresh()
		})
	}
	self.unload = function(command) {
		self.nscp_status().busy('Loading', 'Unloading ' + command.name())
		$.getJSON("/registry/control/module/unload?name="+command.name(), function(data) {
			self.refresh()
		})
	}
	self.update_enable = function(name, status) {
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [];
		payload = {}
		payload['plugin_id'] = 1234
		payload['update'] = {}
		payload['update']['node'] = {}
		payload['update']['node']['path'] = '/modules'
		payload['update']['node']['key'] = name
		payload['update']['value'] = {}
		payload['update']['value']['type'] = 'string'
		payload['update']['value']['value'] = status
		root['payload'].push(payload)
		self.nscp_status().busy('Saving', 'Refresing ' + name + '...')
		$.post("/settings/query.json", JSON.stringify(root), function(data) {
			self.nscp_status().not_busy()
		})
	}
	self.enable_module = function(command) {
		self.update_enable(command.name(), 'enabled')
		self.refresh_settings()
	}
	self.disable_module = function(command) {
		self.update_enable(command.name(), 'disabled')
		self.refresh_settings()
	}
	self.configure_module = function(command) {
		self.currentName(command.name())
		self.module(command)
		self.module().refresh_settings()
		$('.collapse').collapse({parent: '#accordion',  toggle: false})
		$('#collapseDesc').collapse('show')
		$("#module").modal('show');
	}
	self.save_settings = function(command) {
		$("#module").modal('hide');
		root={}
		root['header'] = {};
		root['header']['version'] = 1;
		root['type'] = 'SettingsRequestMessage';
		root['payload'] = [];
		self.keys().forEach(function(e1) {
			e1.forEach(function (entry) {
				if (entry.old_value != entry.value()) {
					root['payload'].push(entry.build_payload())
				}
			})
		})
		if (root['payload'].length > 0) {
			self.nscp_status().busy('Saving', 'Refresing ' + self.currentName() + '...')
			$.post("/settings/query.json", JSON.stringify(root), function(data) {
				self.nscp_status().not_busy()
			})
		} else {
			self.nscp_status().message("warn", "Settings not saved", "No changes detected");
		}
	}
	self.set_default_value = function(key) {
		key.value('')
	}
	self.refresh()
}
root = new CommandViewModel()
ko.applyBindings(root);