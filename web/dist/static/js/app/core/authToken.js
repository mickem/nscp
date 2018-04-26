define([], function() {
	function auth_token_class() {
		var self = this
		self.token = ''
		self.get = function() {
			return self.token;
		}
		self.set = function(token) {
			self.token = token;
		}
		self.register = function(page) {
			self.page = page;
		}
		self.showLogin = function() {
			old = self.page.getOld()
			if (old.name != 'login-page') {
				self.old = self.page.getOld()
			}
			self.router = self.page
			self.page.setRoute('login-page', 'index', 'app/misc/login');
		}
		self.restoreView = function() {
			self.router.restoreOld(self.old)
		}
	}
	
	return new auth_token_class();

});
