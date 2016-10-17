define(['knockout', 'app/core/server', 'app/core/globalStatus', 'app/core/utils', 'app/core/settings', 'app/core/queries'], 
	function(ko, server, gs, u, settings, queries) {

	var id = 0;
	function Module(entry) {
		var self = this;
		self.name = ko.observable(entry['name'])
		self.id = entry['id']
		if (!self.id) {
			self.id = entry['name']
			self.alias = ""
		} else {
			if (entry['name'] == self.id)
				self.alias = ""
			else
				self.alias = self.id
		}
		self.title = entry['info']['title']
		self.desc = entry['info']['description']
		self.is_loaded = ko.observable(false)
		self.is_busy = ko.observable(false)
		self.showDetails = ko.observable(false);
		self.is_enabled = ko.observable(false);
		self.keys = ko.observableArray([]);
		self.queries = ko.observableArray([]);
		self.templates = ko.observableArray([]);
		
		entry['info']['metadata'].forEach(function(entry) {
			if (entry.key == "loaded")
				self.is_loaded(entry.value=="true")
		})
		
		self.load = function() {
			gs.busy('Loading', self.name())
			self.is_busy(true);
			server.json_get("/registry/control/module/load?name="+self.name(), function(data) {
				self.is_loaded(true)
				self.refresh(function() {
					self.is_busy(false);
				})
			})
		}
		self.unload = function() {
			gs.busy('Unloading', self.name())
			self.is_busy(true);
			server.json_get("/registry/control/module/unload?name="+self.name(), function(data) {
				self.is_busy(false);
				self.is_loaded(false);
				self.keys([])
				self.queries([])
				gs.not_busy()
			})
		}
		
		self.refresh = function(on_done) {
			var count = 2;
			self.refresh_settings(function() { 
				count--;
				if (count <=0 ) {
					if (on_done)
						on_done()
				}
			})
			self.refresh_queries(function() { 
				count--;
				if (count <=0 ) {
					if (on_done)
						on_done()
				}
			}, true)
		}
		self.show = function() {
			self.refresh()
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
		
		self.update_enable = function(name, status) {
			settings.save_key('/modules', name, status)
			settings.save()
		}
		self.enable_module = function() {
			self.update_enable(self.name(), 'enabled')
			self.refresh_settings()
		}
		self.disable_module = function(command) {
			self.update_enable(self.name(), 'disabled')
			self.refresh_settings()
		}
		self.refresh_settings = function(on_done) {
			settings.find_templates(function (i) { 
				return i.plugin == self.name() 
			}, function (tpls) { 
				self.templates(tpls)
			})
			
			if (!self.is_loaded() || self.keys.length > 0) {
				if (on_done)
					on_done();
			}
			keys = []
			settings.refresh_keys(function (entry) {
				if (entry.plugs.indexOf(self.name()) != -1) {
					keys.push(entry);
				}
				if (entry.path == '/modules' && entry.key == self.name()) {
					if (entry.value() == "1" || entry.value() == "enabled")
						self.is_enabled(true)
					else
						self.is_enabled(false)
				}
				return entry;
			}, function() {
				result = u.groupBy(keys, function(item) { return [item.path]; })
				self.keys(result);
				if (on_done)
					on_done();
			})
		}
		self.refresh_queries = function(on_done, force) {
			if (!self.is_loaded() || self.queries.length > 0) {
				if (on_done)
					on_done();
			}
			fun = queries.lazy_refresh
			if (force)
				fun = queries.refresh
			fun(function() {
				self.queries(queries.findByModule(self.id))
				if (on_done)
					on_done();
			})
		}
		
		self.save_settings = function() {
			console.log("+++")
			settings.save()
		}
		self.undo_settings = function() {
			settings.undo();
		}
		
	}

	function modules() {
		var self = this;
		self.modules = ko.observableArray([]);
		gs.set_on_logout(function () {self.modules([]);})
		
		settings.add_trigger("/modules", function(path, key, value) {
			if (path == "/modules") {
				self.modules().forEach(function (entry) {
					if (entry.name() == key) {
						if (value == "1" || value == "enabled")
							entry.is_enabled(true)
						else
							entry.is_enabled(false)
					}
				})
			}
		})

		self.refresh_settings = function(on_done) {
			server.json_get("/settings/inventory?path=/modules&recursive=false&keys=true", function(data) {
				if (data['payload'] && data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						name = entry['node']['key']
						var match = ko.utils.arrayFirst(self.modules(), function(item) {
							return item.name() == name;
						});
						if (match) {
							if (entry['value']['string_data'] == "1" || entry['value']['string_data'] == "enabled")
								match.is_enabled(true)
							else
								match.is_enabled(false)
						}
					});
				}
				on_done();
			})
		}
		self.refresh_modules = function(on_done) {
			server.json_get("/registry/inventory/modules", function(data) {
				added_item = false;
				if (data['payload'][0]['inventory']) {
					data['payload'][0]['inventory'].forEach(function(entry) {
						name = entry['name']
						id = entry['id']
						if (!id)
							id = entry['name']
						var match = ko.utils.arrayFirst(self.modules(), function(item) {
							return item.id == id;
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
				on_done();
			})
		}
		self.lazy_refresh = function(cb) {
			if (self.modules().length == 0)
				self.refresh(cb)
			else if (cb)
				cb();
		}
		self.refresh = function(cb) {
			gs.busy('Reloading', 'modules')
			self.refresh_modules(function () {
				self.refresh_settings(function() {
					gs.not_busy()
					if (cb)
						cb()
				});
			});
		}
		self.find = function(id) {
			return ko.utils.arrayFirst(self.modules(), function(item) {
				return item.id == id;
			});
		}

	};
	
	return new modules();

});
