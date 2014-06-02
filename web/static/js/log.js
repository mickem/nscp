
function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(new NSCPStatus(false));
	self.log = ko.observable("");
	self.pos = ko.observable(0);

	self.refresh = function() {
		$.getJSON("/log/messages?pos="+self.pos(), function(data) {
			log = data['log']['data']
			self.pos(data['log']['pos'])
			if (log != "") {
				self.log(log + self.log())
			}
			setTimeout(self.refresh, 1000);
		})
	}
	self.reset = function(command) {
		$.getJSON("/log/reset", function(data) {
			self.log("")
			self.pos(0)
		})
	}
	self.refresh()
}
ko.applyBindings(new CommandViewModel());