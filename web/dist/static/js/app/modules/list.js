define(['jquery', 'knockout', 'text!app/modules/list.html', 'app/core/modules'], function($, ko, templateString, mods) {
	function ModuleListViewModel() {
		var self = this;

		mods.lazy_refresh()
		self.modules = mods.modules;
		self.refresh = function(on_done) {
			mods.refresh()
		}
		self.currentFilter = ko.observable();
		self.filterModules = ko.computed(function() {
			if(!self.currentFilter()) {
				return self.modules(); 
			} else {
				var key = self.currentFilter().toLowerCase()
				console.log(key)
				return ko.utils.arrayFilter(self.modules(), function(m) {
					console.log(m.title.toLowerCase())
					return m.title.toLowerCase().indexOf(key) != -1;
				});
			}
		});
	}
	return { 
		template: templateString, 
		viewModel: ModuleListViewModel 
	};	
})
