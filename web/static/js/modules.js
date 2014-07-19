
function Module(entry) {
	var self = this;

	self.name = entry['name'];
	self.title = entry['info']['title'];
	self.desc = entry['info']['description'];
	self.showDetails = ko.observable(false);
	self.plugin_id = 0
	self.is_loaded = ko.observable(entry['info']['metadata']['loaded']=="true")

	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	
	self.update = function(entry) {
		self.is_loaded(entry['info']['metadata']['loaded']=="true")
	}
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
	self.refresh = function() {
		self.nscp_status().busy('Loading', 'Refresing module list...')
		$.getJSON("/registry/inventory/modules", function(data) {
			added_item = false;
			if (data['payload']['inventory']) {
				data['payload']['inventory'].forEach(function(entry) {
					name = entry['name']
					var match = ko.utils.arrayFirst(self.modules(), function(item) {
						return item.name == name;
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
				self.modules.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
			self.nscp_status().not_busy()
		})
	}
	self.load = function(command) {
		self.nscp_status().busy('Loading', 'Loading ' + command['name'])
		$.getJSON("/registry/control/module/load?name="+command['name'], function(data) {
			self.refresh()
		})
	}
	self.unload = function(command) {
		self.nscp_status().busy('Loading', 'Unloading ' + command['name'])
		$.getJSON("/registry/control/module/unload?name="+command['name'], function(data) {
			self.refresh()
		})
	}
	
	self.refresh()
}
ko.applyBindings(new CommandViewModel());