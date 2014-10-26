function CommandViewModel() {
	var self = this
	self.password = ko.observable();
	self.error_message = ko.observable(getUrlVars()['error'])
	console.log(this.msg)
	
	self.login = function() {
		console.log("Login: " + self.password())
		
		$.getJSON("/auth/token?password="+self.password(), function(data, textStatus, xhr) {
			console.log(xhr.getResponseHeader("TOKEN"));
		})
		
	}
}
ko.applyBindings(new CommandViewModel());
