define(['jquery', 'knockout', 'text!app/modules/list.html', 'app/core/modules'], function($, ko, templateString, mods) {
	function ModuleListViewModel() {
		var self = this;

		mods.lazy_refresh()
		self.modules = mods.modules;
		self.refresh = function(on_done) {
			mods.refresh()
		}
	}
	return { 
		template: templateString, 
		viewModel: ModuleListViewModel 
	};	
})
