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

	function TplEntry(entry) {
		var self = this;
		self.path = entry['node']['path'];
		self.title = entry['info']['title'];
		self.json_value = u.settings_get_value(entry['value'])
		self.raw_data = JSON.parse(self.json_value)
		self.icon = self.raw_data.icon;
		self.desc = self.raw_data.description;
		if (self.raw_data.fields)
			self.fields = self.raw_data.fields;
		else
			self.fields = []
		self.events = self.raw_data.events;
		self.plugin = entry['info']['plugin'];
		if (self.events && self.events.onSave) {
			self.events.onSave = eval(self.events.onSave)
		}
		self.get_field = function(id) {
			for (var i = 0; i < self.fields.length; i++) {
				if (self.fields[i].id == id)
					return self.fields[i];
			}
			return null;
		}
		
		self.fields.forEach(function (item) {
			if (item.type == 'data-choice') {
				item.data = []
				item.fetchData = function findMatches(q, cb) {
					var matches = [];
					q = q.replaceAll("\\", "\\\\")
					q = q.replaceAll("(", "\\(")
					q = q.replaceAll(")", "\\)")
					var substrRegex = new RegExp(q, 'i');
					for (var i=0;i<item.data.length;i++) {
						if (matches.length > 100) {
							cb(matches);
							return;
						}
						if (substrRegex.test(item.data[i])) {
							matches.push({ value: item.data[i] });
						}
					}
					cb(matches);
				}
			}
		});
		
		self.getInstance = function(settings) {
			var instance = self; //jQuery.extend(true, {}, self);
			instance.parent = self
			instance.settings = settings
			instance.save_fields = []
			instance.fields.forEach(function (item) {
				var sf = {
					"id": item.id,
					"key": item.key,
					"parent": item,
					"value": ko.observable()
				}
				instance.save_fields.push(sf)
			})
			instance.get_field = function(id) {
				for (var i = 0; i < instance.save_fields.length; i++) {
					if (instance.save_fields[i].id == id)
						return instance.save_fields[i];
				}
				return null;
			}
			instance.save = function() {
				instance.events.onSave(instance)
				instance.save_fields.forEach(function (item) {
					if (item.key)
						instance.settings.save_key(instance.save_path, item.key, item.value())
				})
				instance.settings.save(function() {
					console.log("saved")
				})
			}
			return instance;
		}
		
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
		self.path = ''
		self.key = ''
		if (entry && entry['node']) {
			if (entry['node'] && entry['node']['path'])
				self.path = entry['node']['path'];
			if (entry['node'] && entry['node']['key'])
				self.key = entry['node']['key'];
		}
		self.advanced = false
		self.plugs = []
		if (entry && entry['info']) {
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
		if (entry && entry['value']) {
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
		self.presist_update = function() {
			self.old_value = self.value()
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
		self.templates = ko.observableArray([])
		self.tpl_map = {}
		self.keys = []
		self.get_templates = function(path, handler) {
			if (self.templates().length == 0) {
				self.refresh_tpls(false, function() {
					handler(self.tpl_map[path])
				})
			} else {
				handler(self.tpl_map[path])
			}
		}
		self.find_templates = function(filter, on_done) {
			if (self.templates().length == 0) {
				self.refresh_tpls(false, function() {
					m = []
					self.templates().forEach(function f(i) {
						if (filter(i))
							m.push(i)
					})
					on_done(m)
				})
			} else {
				m = []
				self.templates().forEach(function f(i) {
					if (filter(i))
						m.push(i)
				})
				on_done(m)
			}
		}
		self.triggers = {}
		self.add_trigger = function(path, cb) {
			self.triggers[path] = cb
		}
		self.touch = function(path, key, value) {
			Object.keys(self.triggers).forEach(function (p) {
				if (path.startsWith(p))
					self.triggers[p](path, key, value)
			})
		}
		self.refresh_paths = function(handler, on_done) {
			gs.busy('Refreshing', 'paths...')
			server.json_get("/settings/inventory?path=/&recursive=true&paths=true", function(data) {
				path_map = {}
				paths = []
				if (data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						var p = new PathEntry(entry)
						if (handler)
							p = handler(p)
						paths.push(p);
						path_map[p.path] = p
					});
				}
				paths.forEach(function (e) {
					if (e.parentPath in path_map) {
						e.set_parent(path_map[e.parentPath])
					} else {
						e.set_parent(self.addParentNode(path_map, e.parentPath))
					}
				})
				self.path_map = path_map;
				self.paths(paths)
				gs.not_busy();
				self.refresh_keys(false, function (keys) {
				if (on_done)
					on_done(self.paths())
				})
			})
		}
		self.refresh_keys = function(handler, on_done) {
			self.lazy_refresh_paths(function() {
				self.do_refresh_keys(handler, on_done)
			})
		}
		self.do_refresh_keys = function(handler, on_done) {
			gs.busy('Refreshing', 'keys...')
			server.json_get("/settings/inventory?path=/&recursive=true&keys=true", function(data) {
				if (data['payload'][0]['inventory']) {
					var keys = []
					var orphaned_keys = {}
					
					data['payload'][0]['inventory'].forEach(function(entry) {
						if (self.path_map[entry['node']['path']]) {
							self.path_map[entry['node']['path']].akeys = []
							self.path_map[entry['node']['path']].keys = []
						}
					})
					data['payload'][0]['inventory'].forEach(function(entry) {
						var p = new KeyEntry(entry)
						if (handler)
							p = handler(p)
						keys.push(p);
						if (self.path_map[p.path]) {
							if (p.advanced)
								self.path_map[p.path].akeys.push(p)
							else
								self.path_map[p.path].keys.push(p)
						} else {
							if (orphaned_keys[p.path])
								orphaned_keys[p.path].push(p)
							else
								orphaned_keys[p.path] = [p]
						}
					});
					self.orphaned_keys = orphaned_keys
					self.keys = keys;
				}
				gs.not_busy();
				if (on_done)
					on_done(self.keys)
			})
		}
		self.refresh_tpls = function(handler, on_done) {
			gs.busy('Refreshing', 'templates...')
			server.json_get("/settings/inventory?path=/&recursive=true&templates=true", function(data) {
			if (data['payload'][0]['inventory']) {
					templates = []
					tpl_map = {}
					data['payload'][0]['inventory'].forEach(function(entry) {
						var p = new TplEntry(entry)
						if (handler)
							p = handler(p)
						templates.push(p);
						if (!tpl_map[p.path])
							tpl_map[p.path] = []
						tpl_map[p.path].push(p)
					});
					self.tpl_map = tpl_map;
					self.templates(templates)
				}
				gs.not_busy();
				if (on_done)
					on_done()
			})
		}
		self.addParentNode = function(path_map, path) {
			var p = new PathEntry(path)
			p.path = path
			path_map[p.path] = p
			if (p.parentPath in path_map) {
				p.set_parent(path_map[p.parentPath])
			} else {
				p.set_parent(self.addParentNode(path_map, p.parentPath))
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
			
			var touched = []
			self.keys.forEach (function (k) {
				if (k.is_dirty()) {
					touched.push(k)
					root['payload'].push(k.build_payload());
				}
			})
			if (root['payload'].length > 0) {
				gs.busy('Saving ', 'data...')
				server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
					if (self.alwaysSave()) {
						console.log("Saving to disk")
						self.saveStore();
					}
					touched.forEach (function (k) {
						k.presist_update()
						self.touch(k.path, k.key, k.value())
					})
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
		self.find_key = function(path, key) {
			for (var i=0;i<self.keys.length;i++) {
				if (self.keys[i].path == path && self.keys[i].key == key)
					return self.keys[i]
				return null;
			}
		}
		self.save_key = function(path, key, value) {
			console.log("Saving: " + path + ": " + key + " = " + value)
			var keyInstance = self.find_key(path, key)
			if (keyInstance)
				keyInstance.value(value)
			else
				self.add_key(path, key, value)
		}
		self.add_key = function(path, key, value) {
			var nk = new KeyEntry()
			nk.path = path;
			nk.key = key
			nk.title = key
			nk.desc = 'UNDEFINED KEY'
			nk.value(value)
			if (self.path_map[nk.path]) {
				self.path_map[nk.path].akeys.push(nk)
			}
			self.keys.push(nk)
		}
		self.add = function(path, key, value, on_done) {
			gs.busy('Adding ', key + '...')
			root={}
			root['type'] = 'SettingsRequestMessage';
			root['payload'] = [ build_payload(path, key, value) ];
			server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
				gs.not_busy()
				self.refresh(on_done);
			})
			self.touch(path, key, value)
		}
			
		self.loadStore = function() {
			gs.busy('Loading', 'Loading data...')
			root={}
			root['header'] = {};
			root['header']['version'] = 1;
			root['type'] = 'SettingsRequestMessage';
			payload = {}
			payload['plugin_id'] = 1234
			payload['control'] = {}
			payload['control']['command'] = 'LOAD'
			root['payload'] = [ payload ];
			
			server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
				gs.not_busy()
				self.refresh()
			})
		}
		self.saveStore = function() {
			gs.busy('Saving', 'Saving data...')
			root={}
			root['header'] = {};
			root['header']['version'] = 1;
			root['type'] = 'SettingsRequestMessage';
			payload = {}
			payload['plugin_id'] = 1234
			payload['control'] = {}
			payload['control']['command'] = 'SAVE'
			
			root['payload'] = [ payload ];
			server.json_post("/settings/query.json", JSON.stringify(root), function(data) {
				gs.not_busy()
				self.refresh()
			})
		}
		self.alwaysSave = ko.observable(false);
		self.toggleSaveAlways = function() {
			self.alwaysSave(!self.alwaysSave())
		}
	};
	
	return new settings();


});
