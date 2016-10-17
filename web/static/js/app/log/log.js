define(['jquery', 'knockout', 'text!app/log/log.html', 'app/core/utils', 'app/core/server', 'app/core/log'], function($, ko, templateString, ut, server, log) {

	function LogViewModel() {
		var self = this;
		
		self.filtersMap = log.filtersMap
		log.set_refresh_handler(function (log) { self.log(log)})
		self.activeFilter = ko.observableArray(['Error', 'Warning', 'Info', 'Debug']);
		self.filters = ko.observableArray(ut.convertToObservable(log.filterLst));

		self.log = ko.observableArray(log.log);

		self.filteredLog = ko.computed(function() {
			var result;
			var funs = []
			ko.utils.arrayForEach(self.activeFilter(), function(item) { funs.push(self.filtersMap[item]) } );
			result = ko.utils.arrayFilter(self.log(), function(item) {
				return funs.some(function(el) { return el(item); })
			});
			return result;
		});
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
	}
	return { 
		template: templateString, 
		viewModel: LogViewModel 
	};	
})
