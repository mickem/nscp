
function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(new NSCPStatus());
}
ko.applyBindings(new CommandViewModel());