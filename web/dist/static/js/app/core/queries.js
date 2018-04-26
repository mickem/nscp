define(['knockout', 'app/core/server', 'app/core/globalStatus', 'app/core/utils', 'handlebars'], function(ko, server, gs, ut, Handlebars) {


	function executeQuery(query, cb) {
		server.json_get("/query/" + query, function(data) {
			r = data['payload'][0]
			lines = []
			r.lines.forEach(function (l) {
				if (l.perf) {
					for (var i=0;i<l.perf.length;i++) {
						if (l.perf[i].int_value) {
							l.perf[i].int_value.r_value = l.perf[i].int_value.value
							l.perf[i].int_value.r_warning = l.perf[i].int_value.warning
							l.perf[i].int_value.r_critical = l.perf[i].int_value.critical
							l.perf[i].int_value.r_minimum = l.perf[i].int_value.minimum
							l.perf[i].int_value.r_maximum = l.perf[i].int_value.maximum
							l.perf[i].value = l.perf[i].int_value
						} else if (l.perf[i].float_value) {
							l.perf[i].float_value.r_value = Math.round(l.perf[i].float_value.value * 100) / 100
							l.perf[i].float_value.r_warning = Math.round(l.perf[i].float_value.warning * 100) / 100
							l.perf[i].float_value.r_critical = Math.round(l.perf[i].float_value.critical * 100) / 100
							l.perf[i].float_value.r_minimum = Math.round(l.perf[i].float_value.minimum * 100) / 100
							l.perf[i].float_value.r_maximum = Math.round(l.perf[i].float_value.maximum * 100) / 100
							l.perf[i].value = l.perf[i].float_value
						}
					}
				} else {
					l.perf = []
				}
				lines.push(l)
			})
			cb(r.result, lines)
		})
	}

	var ids = 0;
	function CommandEntry(entry) {
		var self = this;
		self.name = entry['name'];
		self.desc = entry['info']['description'];
		self.plugs = entry['info']['plugin'];
		self.command = ko.observable(self.name)
		self.resultStatus = ko.observable(false)
		self.suggestion = Handlebars.compile('<p><strong>{{key}}</strong>: {{tip}}</p>')
		self.resultLines = ko.observableArray([])
		self.params = []
		self.id = self.name
		if (entry['parameters']['parameter']) {
			entry['parameters']['parameter'].forEach(function(entry) {
				entry.first_line = entry.short_description
				if (entry.long_description)
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
		self.execute = function() {
			executeQuery(ut.parseCommand(self.command()), function(status, lines) {
				self.resultStatus(status)
				self.resultLines(lines)
			})
		}
	}


	function queries() {
		var self = this;
		self.commands = ko.observableArray([]);
		self.refresh = function(on_done) {
			gs.busy('Refreshing', 'queries...')
			server.json_get("/registry/inventory", function(data) {
				if (data['payload'][0] && data['payload'][0]['inventory']) {
					ids=0
					self.commands.removeAll()
					data['payload'][0]['inventory'].forEach(function(entry) {
						self.commands.push(new CommandEntry(entry));
					});
				}
				self.commands.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
				gs.not_busy();
				if (on_done) {
					on_done();
				}
			})
		}
		self.lazy_refresh = function(on_done) {
			if (self.commands().length > 0) {
				if (on_done)
					on_done();
			} else {
				self.refresh(on_done);
			}
		}
		self.find = function(id) {
			return ko.utils.arrayFirst(self.commands(), function(item) {
				return item.id == id;
			});
		}
		self.findByModule = function(mod) {
			commands = []
			self.commands().forEach(function(e) {
				if (e.plugs.indexOf(mod) === 0) {
					commands.push(e)
				}
			})
			return commands;
		}
		self.execute = executeQuery
	}
	return new queries;
})