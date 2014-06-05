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

function NSCPStatus(state) {
	var self = this;
	self.poller_state = typeof state !== 'undefined' ? state : true;
	self.has_issues = ko.observable('')
	self.error_count = ko.observable('')
	self.last_error = ko.observable('')
	self.busy_header = ko.observable('')
	self.busy_text = ko.observable('')
	self.poller = null;
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
	self.update = function(elem) {
		self.error_count(elem['count'])
		self.last_error(elem['error'])
		self.has_issues(self.error_count() > 0)
	}
	self.busy = function(header, text) {
		if (self.poller_state)
			self.cancelPoller();
		self.busy_header(header)
		self.busy_text(text)
		$("#busy").modal({"backdrop" : "static", "show": "true"});
	}
	self.not_busy = function() {
		$("#busy").modal('hide');
		self.busy_header('')
		self.busy_text('')
		if (self.poller_state)
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
	
		self.message_header(header)
		self.message_text(text)
		$("#busy").modal({"backdrop" : "hide", "show": "true"});
	}
		
	self.do_update = function(elem) {
		$.getJSON("/log/status", function(data) {
			self.update(data['status']);
		})
	}
	self.poll = function() {
		self.do_update();
		self.poller = setTimeout(self.poll, 5000);
	}

	self.reset = function() {
		$.getJSON("/log/reset", function(data) {
			self.do_update();
		})
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
					self.not_busy();
				} else {
					self.restart_waiter = setTimeout(self.restart_poll, 1000);
				}
            },
			error: function(jqXHR, textStatus, errorThrown) {
				self.restart_waiter = setTimeout(self.restart_poll, 1000);
			},
			dataType: 'json'
        });
	}
	self.restart = function() {
		self.busy("Restarting...", "Please wait while we restart...");
		$.getJSON("/core/reload", function(data) {
			self.schedule_restart_poll();
		})
	}
	

	self.start = function() {
		self.poll();
	}
	if (self.poller_state) {
		self.start()
	}
}

