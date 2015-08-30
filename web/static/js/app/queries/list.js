define(['knockout', 'text!app/queries/list.html', 'app/core/queries'], function(ko, templateString, queries) {


	function QueryViewModel() {
		var self = this;

		self.commands = queries.commands;

		self.refresh = function() {
			queries.refresh();
		}
		queries.lazy_refresh();
		
	}
	return { 
		template: templateString, 
		viewModel: QueryViewModel 
	};	
})

