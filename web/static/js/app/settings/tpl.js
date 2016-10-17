define(['knockout', 'text!app/settings/tpl.html', 'app/core/settings', 'app/core/utils', 'app/core/execs', 'handlebars', 'app/core/globalStatus', 'typeahead.jquery'], 
	function(ko, templateString, settings, ut, exe, Handlebars, gs) {
	var nodeMap = {}

	function TplViewModel(param) {
		var self = this;
		self.fetching = ko.observable(0)
		self.template = ko.observable()

		self.path = "/"
		self.index = 0
		if (param && param.splat && param.splat.length > 0) {
			self.path = param.splat[0]
			var n = self.path.lastIndexOf('/');
			self.index = self.path.substring(n + 1);
			self.path = self.path.substring(0, n);
		}
		
		settings.get_templates(self.path, function(tpls) { 
			if (tpls[self.index]) {
				tpl = tpls[self.index].getInstance(settings)
				tpl.fields.forEach(function (item){
					if (item.type == 'data-choice') {
						if (item.data.length === 0) {
							self.fetching(self.fetching()+1);
							exe.execute(ut.parseExecCommand(item.exec), function(s, m) {
								if (s != 0 && s != "OK") {
									gs.warning("Failed to execute query...")
								}
								self.fetching(self.fetching()-1);
								item.data = JSON.parse(m)
							})
						}
					}
				})
				self.template(tpl)
			}
		})


		self.save = function() {
			settings.save(function () {
			});
		}
	}
	return { 
		template: templateString, 
		viewModel: TplViewModel 
	};	
})

