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

function CommandViewModel() {
	var self = this;
	
	self.filtersMap = {
					'Error': function(item){return item.type == 'error'; },
					'Warning': function(item){return item.type == 'warning'; },
					'Info': function(item){return item.type == 'info'; },
					'Debug': function(item){return item.type == 'debug'; },
					'Console': function(item){return item.type == 'out'; }
	};
	filterLst = [
					 { title:'Error', css: 'error', filter: function(item){return item.type == 'error';}},
					 { title:'Warning', css: 'warning', filter: function(item){return item.type == 'warning';}},
					 { title:'Info', css: 'info', filter: function(item){return item.type == 'info'; }},
					 { title:'Debug', css: 'debug', filter: function(item){return item.type == 'debug'; }}
	]
	self.activeFilter = ko.observableArray(['Error', 'Warning', 'Info', 'Debug']);
	if (!log_direction) {
		filterLst.push({ title:'Console', css: 'console', filter: function(item){return item.type == 'out';}})
		self.activeFilter.push('Console')
	}
	self.filters = ko.observableArray(convertToObservable(filterLst));
	


	self.nscp_status = ko.observable(new NSCPStatus(false));
	self.log = ko.observableArray([]);
	self.pos = ko.observable(0);
	self.command = ko.observable('');
	self.command.extend({ notify: 'always' });

	self.filteredLog = ko.computed(function() {
		var result;
		var funs = []
		ko.utils.arrayForEach(self.activeFilter(), function(item) { funs.push(self.filtersMap[item]) } );
		result = ko.utils.arrayFilter(self.log(), function(item) {
			return funs.some(function(el) { return el(item); })
		});
		return result;
	});
	if (!log_direction)
		self.filteredLog.extend({ scrollFollow: '#logScroll' })
	self.setActiveFilter = function(model,event){
		key = model.title()
		if (self.activeFilter.indexOf(key) == -1) {
			self.activeFilter.push(key);
			model.css(model.title().toLowerCase())
		} else {
			self.activeFilter.remove(key);
			model.css('inactive')
		}
	};
	self.rev = log_direction;
	self.refreshOne = function(done) {
		$.getJSON("/log/messages?pos="+self.pos(), function(data) {
			self.pos(data['log']['pos'])
			data['log']['data'].forEach(function(entry) {
				if (self.rev) {
					self.log.unshift(new LogEntry(entry))
				} else {
					self.log.push(new LogEntry(entry))
				}
			});
			done()
		})
	}
	
	self.refresh = function() {
		self.refreshOne(function() {setTimeout(self.refresh, 1000)})
	}
	self.reset = function(command) {
		$.getJSON("/log/reset", function(data) {
			self.log("")
			self.pos(0)
		})
	}
	self.exec = function(event){
		command = self.command()
		history_log.unshift(command)
		history_index = -1
		self.nscp_status().busy('Executing...', command)
		self.command('')
		$.getJSON("/console/exec?command="+encodeURIComponent(command), function(data) {
			self.nscp_status().not_busy()
			self.refreshOne(function() {})
		})
	}
	 self.action = function(event){
		if (history_log.length > 0) {
			 if (event.keyCode == 38) {
				if (history_index == -1) {
					history_save = self.command()
					console.log(history_save)
					history_index = 0
				} else if (history_index < history_log.length)
					history_index++
				self.command(history_log[history_index])
			} else {
				if (history_index == 0) {
					history_index = -1
					self.command(history_save)
				} else if (history_index > 0) {
					history_index--
					self.command(history_log[history_index])
				}
			}
		}
		event.stopPropagation()
	};
	self.refresh()
}
ko.applyBindings(new CommandViewModel());