define(['knockout', 'text!app/misc/login.html', 'app/core/globalStatus', 'app/core/router', 'app/core/authToken'], function(ko, templateString, gs, router, auth) {
	
	function LoginViewmodel() {
		var self = this;
		self.nscp_status = gs;
		self.password = ko.observable('');
		self.login_error_message = ko.observable('')
		
		self.login = function() {
			$.getJSON("/auth/token?password="+encodeURIComponent(self.password()), function(data, textStatus, xhr) {
				console.log("Is logged in")
				auth.set(xhr.getResponseHeader("__TOKEN"));
				gs.is_loggedin(true)
				gs.on_login.forEach(function (handler) { handler(); });
				gs.do_update();
				auth.restoreView()
			}).error(function(xhr, error, status) {
				self.login_error_message(xhr.responseText)
			})
		}
	}

	return { template: templateString, viewModel: LoginViewmodel };
});