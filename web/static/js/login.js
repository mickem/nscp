function CommandViewModel() {
	this.error_message = getUrlVars()['error']
	console.log(this.msg)
}
ko.applyBindings(new CommandViewModel());
