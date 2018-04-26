define(['jquery', 'knockout', 'text!app/log/console.html', 'app/core/utils', 'app/core/server', 'app/core/log'], function($, ko, templateString, ut, server, log) {
	var history_log = []
	var history_index = -1
	var history_save = ''

	function LogViewModel() {
		var self = this;
		
		self.log = ko.observableArray(log.log);

		self.filtersMap = log.filtersMap
		log.set_refresh_handler(function (log) {
			self.log(log.reverse())
		})
		self.activeFilter = ko.observableArray(['Error', 'Warning', 'Info', 'Debug']);
		filterLst = log.filterLst
		filterLst.push({ title:'Console', css: 'console', filter: function(item){return item.type == 'out';}})
		self.filters = ko.observableArray(ut.convertToObservable(filterLst));
		self.activeFilter = ko.observableArray(['Error', 'Warning', 'Info', 'Debug']);
		self.activeFilter.push('Console')
		self.filters = ko.observableArray(ut.convertToObservable(filterLst));
		


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
		
		self.reset = function(command) {
			log.clear()
		}
		self.dispose = function() {
			log.set_refresh_handler(false);
		} 
		self.exec = function(){
			command = self.command()
			history_log.unshift(command)
			history_index = -1
			self.command('')
			log.exec(command)
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
	}
	return { 
		template: templateString, 
		viewModel: LogViewModel 
	};	
})
