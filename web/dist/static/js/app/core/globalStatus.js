define(['knockout', 'jquery', 'app/core/authToken', 'app/core/server', 'app/core/utils', 'app/core/router'], function(ko, $, auth, server, ut, router) {


	function SettingsStatus(elem) {
		var self = this;
		self.has_changed = ko.observable('')
		self.context = ko.observable('')
		self.type = ko.observable('')
		self.showMore = function() {
			self.showDetails(!self.showDetails());
		}
		self.update = function(elem) {
			self.context(elem['context'])
			self.type(elem['type'])
			self.has_changed(elem['has_changed'])
		}
		self.refresh = function() {
			server.json_get("/settings/status", function(data) {
				self.update(data['payload'][0]['status'])
			})
		}	
	}

	function NSCPStatus(state) {
		var self = this;
		self.poller_state = typeof state !== 'undefined' ? state : true;
		self.has_issues = ko.observable('')
		self.is_loggedin = ko.observable(auth.get()!='')
		self.error_count = ko.observable('')
		self.last_error = ko.observable('')
		self.busy_header = ko.observable('')
		self.busy_text = ko.observable('')
		self.is_busy = ko.observable(false)
		self.busy_warning = ko.observable(false)
		self.settings = ko.observable(new SettingsStatus());
		
		self.poller = null;
		self.on_login = []
		self.set_on_login = function(f) { self.on_login.push(f)} 
		self.on_logout = []
		self.set_on_logout = function(f) { self.on_logout.push(f) } 
		
		self.refresh_handlers = {}
		
		self.add_refresh_handler = function(id, handler) {
			handler()
			self.refresh_handlers[id] = handler;
		}
		self.remove_refresh_handler = function(id, handler) {
			delete self.refresh_handlers[id];
		}

		self.logout = function() {
			self.is_loggedin(false);
			auth.showLogin();
		}
		
		
		self.showMore = function() {
			self.showDetails(!self.showDetails());
		}
		self.update = function(elem) {
			self.error_count(elem['count'])
			self.last_error(elem['error'])
			self.has_issues(self.error_count() > 0)
		}
		self.set_error = function(text) {
			self.error_count(self.error_count()+1)
			self.last_error(text)
			self.has_issues(self.error_count() > 0)
		}
		self.busy = function(header, text) {
			if (self.poller_state)
				self.cancelPoller();
			self.busy_warning(false)
			self.busy_header(header)
			self.busy_text(text)
			self.is_busy(true)
		}
		self.warning = function(header, text) {
			self.busy_warning(true)
			self.busy_header(header)
			self.busy_text(text)
			self.is_busy(true)
			setTimeout(function() {
				self.not_busy()
			}, 5000);
		}
		self.not_busy = function(start_poller) {
			start_poller = typeof start_poller !== 'undefined' ? start_poller : true;
			self.is_busy(false)
			self.busy_header('')
			self.busy_text('')
			if (self.poller_state && start_poller)
				self.start();
		}
		
		self.message = function(type, title, message) {
			if (type == "info")
				type = BootstrapDialog.TYPE_INFO
			if (type == "warn")
				type = BootstrapDialog.TYPE_WARNING
			BootstrapDialog.show({
				type: type,
				title: title,
				message: message,
				buttons: [{
					label: 'Close',
					action: function(dialogItself){
						dialogItself.close();
					}
				}]
			});	
		}

		self.do_update = function() {
			server.json_get("/log/status", function(data) {
				self.update(data['status']);
			}).error(function(xhr, error, status) {
				if (xhr.status == 200)
					return;
				if (xhr.status == 403) {
					global_status.is_loggedin(false);
				}
				global_status.set_error(xhr.responseText)
				global_status.not_busy(false)
			})
			for (var key in self.refresh_handlers) { 
				f = self.refresh_handlers[key];
				if (f)
					f()
			}
			self.settings_refresh()
		}
		self.poll = function() {
			if (self.is_loggedin()) {
				self.do_update();
			}
			clearTimeout(self.poller)
			self.poller = setTimeout(self.poll, 5000);
		}

		self.reset = function() {
			 if ($.active == 0) {
				server.json_get("/log/reset", function(data) {
					self.do_update();
				})
			 }
		}
		self.cancelPoller = function() {
			clearTimeout(self.poller);
		}
		self.schedule_restart_poll = function() {
			self.restart_waiter = setTimeout(self.restart_poll, 1000);
		}
		self.restart_poll = function() {
			done = false;
			$.ajax({
				type: 'GET',
				url: '/core/isalive',
				data: $("#addContactForm").serialize(),
				cache: false,
				success: function (data, status, xhttp) {
					if (data['status'] && data['status'] == "ok") {
						clearTimeout(self.restart_waiter);
						self.not_busy(true);
					} else {
						self.restart_waiter = setTimeout(self.restart_poll, 1000);
					}
				},
				error: function(jqXHR, textStatus, errorThrown) {
					self.set_error("Failed to connect to backend")
					clearTimeout(self.restart_waiter);
					self.restart_waiter = setTimeout(self.restart_poll, 1000);
				},
				dataType: 'json'
			});
		}
		self.reload = function() {
			self.busy("Restarting...", "Please wait while we restart...");
			server.json_get("/core/reload", function(data) {
				self.schedule_restart_poll();
			})
		}
		self.restart = function() {
			self.busy("Restarting...", "Please wait while we restart...");
			server.json_get("/core/reload", function(data) {
				self.schedule_restart_poll();
			})
		}
		self.settings_refresh = function() {
			self.settings().refresh();
		}

		self.start = function() {
			self.poll();
		}
		if (self.poller_state) {
			self.start()
		}
		self.start()
	}

	var global_status = new NSCPStatus();
	return global_status;
});
