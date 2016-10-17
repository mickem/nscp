define(['knockout', 'app/core/server', 'app/core/globalStatus'], function(ko, server, gs) {
	function LogEntry(entry) {
		var self = this;
		self.line = entry['line'];
		self.file = entry['file'];
		self.type = entry['type'];
		self.date = entry['date'];
		self.link = null
		self.message = entry['message'];
		
		pos = self.file.indexOf("\\nscp\\")
		if (pos == -1) {
			pos = self.file.indexOf("/nscp/")
		}
		if (pos != -1) {
			self.file = "/" + self.file.substr(pos+6)
			self.link = 'https://github.com/mickem/nscp/blob/master' + self.file + "#L" + self.line
		}
		
		self.showDetails = ko.observable(false);
		self.showMore = function() {
			if (self.file.length > 0)
				self.showDetails(!self.showDetails());
		}
		
	}

	var history_log = []
	var history_index = -1
	var history_save = ''

	function LogHandler() {
		var self = this;
		
		self.filtersMap = {
			'Error': function(item){return item.type == 'error'; },
			'Warning': function(item){return item.type == 'warning'; },
			'Info': function(item){return item.type == 'info'; },
			'Debug': function(item){return item.type == 'debug'; },
			'Console': function(item){return item.type == 'out'; }
		};
		self.filterLst = [
			 { title:'Error', css: 'error', filter: function(item){return item.type == 'error';}},
			 { title:'Warning', css: 'warning', filter: function(item){return item.type == 'warning';}},
			 { title:'Info', css: 'info', filter: function(item){return item.type == 'info'; }},
			 { title:'Debug', css: 'debug', filter: function(item){return item.type == 'debug'; }}
		]

		self.log = [];
		self.pos = 0;
		self.command = ko.observable('');
		self.command.extend({ notify: 'always' });

		self.refresh_handler = false;
		self.set_refresh_handler = function(handler) {
			self.refresh_handler = handler;
			if (!handler)
				gs.remove_refresh_handler('log')
			else
				gs.add_refresh_handler('log', function() {
					server.json_get("/log/messages?pos="+self.pos, function(data) {
						self.pos = data['log']['pos']
						var changed = false;
						data['log']['data'].forEach(function(entry) {
							self.log.unshift(new LogEntry(entry))
							changed = true;
						});
						if (changed)
							handler(self.log)
					})
				});
		}
		self.clear = function() {
			server.json_get("/log/reset", function(data) {
				self.log = []
				self.pos = 0
				if (self.refresh_handler)
					self.refresh_handler(self.log)
			})
		}
		self.exec = function(command){
			history_log.unshift(command)
			history_index = -1
			gs.busy('Executing...', command)
			server.json_get("/console/exec?command="+encodeURIComponent(command), function(data) {
				gs.not_busy()
				gs.do_update()
			})
		}
	}
	return new LogHandler;
})
