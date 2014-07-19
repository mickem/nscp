function Module(entry) {
	var self = this;
	self.name = ko.observable()
	self.title = ko.observable()
	self.desc = ko.observable()
	self.is_loaded = ko.observable(false)
	self.update = function(entry) {
		self.name(entry['name'])
		self.title(entry['info']['title'])
		self.desc(entry['info']['description'])
		self.is_loaded(entry['info']['metadata']['loaded']=="true")
	}
}

var refresh_count = 0
function done_refresh(self, count) {
	if (!count)
		count = 1
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
	self.currentName = ko.observable(getUrlVars()['module'])
	self.module = ko.observable(new Module())
	self.paths = ko.observableArray([]);
	self.keys = ko.observableArray([]);
	
	self.refresh = function() {
		init_refresh(self)
		$.getJSON("/registry/inventory/modules?name=" + self.currentName(), function(data) {
			if (data['payload']['inventory']) {
				data['payload']['inventory'].forEach(function(entry) {
					if (entry['name'] == self.currentName())
						self.module().update(entry);
				});
			}
			done_refresh(self);
		})
		$.getJSON("/settings/inventory?path=/&recursive=true&keys=true&module=" + self.currentName(), function(data) {
			keys = []
			if (data['payload'][0]['inventory']) {
				data['payload'][0]['inventory'].forEach(function(entry) {
					if (entry['info']['plugin'].indexOf(self.currentName()) != -1) {
						console.log(entry)
						keys.push(new KeyEntry(entry));
					}
				});
			}
			//self.modules.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
			result = groupBy(keys, function(item) { return [item.path]; })
			console.log(result)
			self.keys(result);
			done_refresh(self);
		})
	}
	self.save_settings = function(command) {
		init_refresh(self);
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
			$.post("/settings/query.json", JSON.stringify(root), function(data) {
				self.refresh()
				done_refresh(self, 2);
			})
		} else {
			done_refresh(self, 2);
			self.nscp_status().message("warn", "Settings not saved", "No changes detected");
		}
	}
	self.set_default_value = function(key) {
		key.value('') // key.default_value
	}
	
	self.refresh()
}
ko.applyBindings(new CommandViewModel());