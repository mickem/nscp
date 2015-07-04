
function CommandViewModel() {
	var self = this;

	self.nscp_status = ko.observable(global_status);
}
ko.applyBindings(new CommandViewModel());


