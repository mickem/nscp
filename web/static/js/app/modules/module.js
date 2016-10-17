define(['jquery', 'knockout', 'text!app/modules/module.html', 'app/core/modules', 'app/core/settings'], 
	function($, ko, templateString, modules, settings) {

	function CommandViewModel(params) {
		var self = this;

		self.module = ko.observable()
		modules.lazy_refresh(function() {
			mod = modules.find(params.id)
			mod.show()
			self.module(mod)
		})

		self.refresh = function(on_done) {
			modules.refresh()
		}
/*
		self.configure_module = function(command) {
			self.currentName(command.name())
			self.module(command)
			self.module().refresh_settings()
			$('.collapse').collapse({parent: '#accordion',  toggle: false})
			$('#collapseDesc').collapse('show')
			$("#module").modal('show');
		}
		*/
	}
	return { 
		template: templateString, 
		viewModel: CommandViewModel 
	};	
})
