define(['knockout', 'app/core/server', 'app/core/globalStatus', 'app/core/utils'], function(ko, server, gs, u) {


	function build_payload(path, key, value) {
		payload = {}
		payload['plugin_id'] = 1234
		payload['update'] = {}
		payload['update']['node'] = {}
		payload['update']['node']['path'] = path
		payload['update']['node']['key'] = key
		payload['update']['value'] = {}
		payload['update']['value']['string_data'] = value
		return payload
	}

	function PathEntry(entry) {
		var self = this;
		
		if (typeof entry == "string") {
			self.path = entry
			self.title = ""
			self.desc = ""
		} else if (typeof entry == "object") {
			self.path = entry['node']['path'];
			self.title = entry['info']['title'];
			self.desc = entry['info']['description'];
			self.plugs = entry['info']['plugin'];
		}
		self.showDetails = ko.observable(false);
		
		self.parentPath = "/"
		self.children = []
		self.akeys = []
		self.keys = []
		self.name = "root"
		trail = ""
		self.path.split('/').forEach(function(entry) {
			if (entry.length > 0) {
				self.name = entry
				//self.paths.push(new PathNode(trail, entry))
				if (trail.length > 0) {
					self.parentPath = trail
				}
				trail = trail + "/" + entry
			}
		});
		if (!self.title) {
			self.title = self.name
		}
		if (!self.desc) {
			self.desc = 'Unknown section for: ' + self.path
		}
		self.set_parent = function(parent) {
			self.parent = parent
			if (self.path == parent.path) {
				return
			}
			for (var i=0; i<parent.children.length; i++) {
				if (parent.children[i].path == self.path) {
					return
				}
			}
			parent.children.push(self)
		}
		
		self.showMore = function() {
			self.showDetails(!self.showDetails());
		}
	}

	function KeyEntry(entry) {
		var self = this;
		self.value = ko.observable('')
		self.is_default = ko.observable(false)

		self.default_value = ''
		self.type = 'string'
		self.path = entry['node']['path'];
		self.key = ''
		self.advanced = false
		self.plugs = []
		if (entry['node']['key'])
			self.key = entry['node']['key'];
		if (entry['info']) {
			info = entry['info']
			if (info['title'])
				self.title = info['title'];
			else
				self.title = 'UNDEFINED KEY'
			if (info['description'])
				self.desc = info['description'];
			else
				self.desc = 'UNDEFINED KEY'
			if (info['plugin'])
				self.plugs = info['plugin'];
			self.advanced = info['advanced'];
			if (info['default_value']) {
				self.default_value = u.settings_get_value(info['default_value']);
				self.value(self.default_value)
			}
		}
		if (entry['value']) {
			self.value(u.settings_get_value(entry['value']))
		}
		if (self.value() == self.default_value) {
			self.is_default(true)
		}
		self.old_value = self.value()

		self.is_dirty = function() {
			return self.value() != self.old_value;
		}
		self.set_empty = function() {
			self.value('')
		}
		self.value.subscribe(function(newValue) {
			if (newValue == self.default_value) {
				self.is_default(true)
			} else {
				self.is_default(false)
			}
		});

		self.has_changed = function() {
			return self.value() != self.old_value;
		}
		self.undo_value = function() {
			self.value(self.old_value);
		}
		self.set_default_value = function() {
			self.value(self.default_value);
		}
		
		self.build_payload = function() {
			return build_payload(self.path, self.key, self.value())
		}
	}


	function settings() {
		var self = this;
		self.fetch = function(query, handler, on_done) {
			//gs.busy('Refreshing', 'keys...')
			server.json_get("/settings/inventory?" + query, function(data) {
				//gs.not_busy();
				if (data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						handler(new KeyEntry(entry))
					});
				}
				if (on_done)
					on_done()
			})
		}
		self.path_map = {}
		self.orphaned_keys = {}
		self.paths = ko.observableArray([])
		self.keys = []
		self.refresh_paths = function(handler, on_done) {
			gs.busy('Refreshing', 'paths...')
			server.json_get("/settings/inventory?path=/&recursive=true&paths=true", function(data) {
				if (data['payload'][0]['inventory']) {
					paths = []
					path_map = {}
					data['payload'][0]['inventory'].forEach(function(entry) {
						p = new PathEntry(entry)
						if (handler)
							p = handler(p)
						paths.push(p);
						path_map[p.path] = p
					});
					self.path_map = path_map;
					self.paths(paths)
				}
				self.paths().forEach(function (e) {
					if (e.parentPath in self.path_map) {
						e.set_parent(self.path_map[e.parentPath])
					} else {
						e.set_parent(self.addParentNode(e.parentPath))
					}
				})
				gs.not_busy();
				self.refresh_keys(false, function (keys) {
				if (on_done)
					on_done(self.paths())
				})
			})
		}
		self.refresh_keys = function(handler, on_done) {
			gs.busy('Refreshing', 'keys...')
			server.json_get("/settings/inventory?path=/&recursive=true&keys=true", function(data) {
				if (data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						p = new KeyEntry(entry)
						if (handler)
							p = handler(p)
						self.keys.push(p);
						if (self.path_map[p.path]) {
							if (p.advanced)
								self.path_map[p.path].akeys.push(p)
							else
								self.path_map[p.path].keys.push(p)
						} else {
							if (self.orphaned_keys[p.path])
								self.orphaned_keys[p.path].push(p)
							else
								self.orphaned_keys[p.path] = [p]
						}
					});
				}
				gs.not_busy();
				if (on_done)
					on_done(self.keys)
			})
		}
		self.addParentNode = function(path) {
			p = new PathEntry(path)
			p.path = path
			self.path_map[p.path] = p
			if (p.parentPath in self.path_map) {
				p.set_parent(self.path_map[p.parentPath])
			} else {
				p.set_parent(self.addParentNode(p.parentPath))
			}
			return p
		}
		self.lazy_refresh_paths = function(on_done) {
			if (self.paths().length > 0) {
				if (on_done)
					on_done([])
				return;
			}
			self.refresh_paths(false, on_done);
		}
		self.get = function(path, on_done) {
			if (on_done) {
				self.lazy_refresh_paths(function(paths) {
					on_done(self.path_map[path])
				})
			}
			return self.path_map[path]
		}
		self.refresh = function(on_done) {
			self.refresh_paths(false, on_done);
		}
		self.save = function(on_done) {
			root={}
			root['type'] = 'SettingsRequestMessage';
			root['payload'] = [];
			
			self.keys.forEach (function (k) {
				if (k.is_dirty()) {
					root['payload'].push(k.build_payload());
				}
			})
			if (root['payload'].length > 0) {
				gs.busy('Saving ', 'data...')
				server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
					self.old_value = self.value()
					gs.not_busy()
					if (on_done)
						on_done()
				})
			} else {
				gs.warning('No changes', 'Cant save settings')
			}
		}
		self.undo = function(on_done) {
			self.keys.forEach (function (k) {
				if (k.is_dirty()) {
					k.undo_value();
				}
			})
		}
		self.add = function(path, key, value, on_done) {
			gs.busy('Saving ', 'data...')
			root={}
			root['type'] = 'SettingsRequestMessage';
			root['payload'] = [ build_payload(path, key, value) ];
			server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
				gs.not_busy()
				self.refresh(on_done);
			})
		}
		
		
	};
	
	return new settings();


});
