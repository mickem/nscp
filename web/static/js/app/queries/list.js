define(['knockout', 'text!app/queries/list.html', 'app/core/queries'], function(ko, templateString, queries) {


	function QueryViewModel() {
		var self = this;

		self.commands = queries.commands;

		self.refresh = function() {
			queries.refresh();
		}
		queries.lazy_refresh();
		self.currentFilter = ko.observable();
		self.filterCommands = ko.computed(function() {
			if(!self.currentFilter()) {
				return self.commands(); 
			} else {
				var key = self.currentFilter().toLowerCase()
				return ko.utils.arrayFilter(self.commands(), function(m) {
					return m.name.toLowerCase().indexOf(key) != -1;
				});
			}
		});
	}
	return { 
		template: templateString, 
		viewModel: QueryViewModel 
	};	
})

