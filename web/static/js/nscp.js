function parseNagiosResult(id) {
	if (id == 0)
		return "OK"
	if (id == 1)
		return "WARNING"
	if (id == 2)
		return "CRITCAL"
	if (id == 3)
		return "UNKNOWN"
	return "INVALID(" + id + ")"
}
function parseNagiosResultCSS(id) {
	if (id == 0)
		return "btn-success"
	if (id == 1)
		return "btn-warning"
	if (id == 2)
		return "btn-danger"
	return "btn-info"
}

function getUrlVars() {
    var vars = [], hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for(var i = 0; i < hashes.length; i++)
    {
        hash = hashes[i].split('=');
        vars.push(hash[0]);
        vars[hash[0]] = hash[1];
    }
    return vars;
}

function NSCPStatus(elem) {
	var self = this;
	self.has_issues = ko.observable('')
	self.error_count = ko.observable('')
	self.last_error = ko.observable('')
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	self.update = function(elem) {
		self.error_count(elem['count'])
		self.last_error(elem['error'])
		self.has_issues(self.error_count() > 0)
	}
	
	self.do_update = function(elem) {
		$.getJSON("/log/status", function(data) {
			self.update(data['status']);
		})
	}
	self.poll = function() {
		self.do_update();
		setTimeout(self.poll, 5000);
	}

	self.reset = function() {
		$.getJSON("/log/reset", function(data) {
			self.do_update();
		})
	}
	
	self.start = function() {
		self.poll();
	}
	self.start()
}

