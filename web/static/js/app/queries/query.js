define(['knockout', 'text!app/queries/query.html', 'app/core/queries', 'typeahead.jquery'], function(ko, templateString, queries) {

	function QueryViewModel(params) {
		var self = this;

		self.query = ko.observable();

		queries.lazy_refresh(function() {
			self.query(queries.find(params.id))
		})
		
		
		self.addArgument = function(item, event) {
			var e = item.getArgument(event.val)
			if (e == null)
				return
			var type = e['content_type']
			if (type == 'STRING')
				self.query().command(self.query().command() + " " + event.val + '="" ')
			else
				self.query().command(self.query().command() + " " + event.val + " ")
			$("#command").focus()
		}
	}
	return { 
		template: templateString, 
		viewModel: QueryViewModel 
	};	
})

