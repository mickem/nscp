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


settings_get_value = function (val) {
	if (val.string_data)
		return val.string_data
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
	
		//$("#busy").modal({"backdrop" : "hide", "show": "true"});
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

function toggleChevron(e) {
    $(e.target)
        .prev('.panel-heading')
        .find("i.indicator")
        .toggleClass('glyphicon-chevron-down glyphicon-chevron-up');
}

function convertToObservable(list) 
{ 
    var newList = []; 
    $.each(list, function (i, obj) {
        var newObj = {}; 
        Object.keys(obj).forEach(function (key) { 
            newObj[key] = ko.observable(obj[key]); 
        }); 
        newList.push(newObj); 
    }); 
    return newList; 
}

function PathNode(parent, path) {
	var self = this;
	self.href = "/settings.html?path=" + parent + "/" + path
	self.title = "Show all keys under: " + parent + "/" + path
	self.name = path
	self.path = parent + "/" + path
}


function make_paths_from_string(path) {
	paths = []
	trail = "";
	path.split('/').forEach(function(entry) {
		if (entry.length > 0) {
			self.paths.push(new PathNode(trail, entry))
			trail = trail + "/" + entry
		}
	});
	return paths
}

function groupBy( array , f )
{
  var groups = {};
  array.forEach( function( o )
  {
    var group = JSON.stringify( f(o) );
    groups[group] = groups[group] || [];
    groups[group].push( o );  
  });
  return Object.keys(groups).map( function( group )
  {
    return groups[group]; 
  })
}

function PathEntry(entry) {
	var self = this;
	
	self.path = entry['node']['path'];
	self.title = entry['info']['title'];
	self.href = "/settings.html?path=" + self.path
	self.paths = make_paths_from_string(self.path)
	self.desc = entry['info']['description'];
	self.plugs = entry['info']['plugin'];
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}

function KeyEntry(entry) {
	var self = this;
	self.value = ko.observable('')

	if (entry['value'])
		self.value(settings_get_value(entry['value']))
	self.path = entry['node']['path'];
	self.key = ''
	if (entry['node']['key'])
		self.key = entry['node']['key'];
	self.paths = make_paths_from_string(self.path)
	self.title = entry['info']['title'];
	if (entry['info']['description'])
		self.desc = entry['info']['description'];
	else
		self.desc = 'UNDEFINED KEY'
	self.plugs = entry['info']['plugin'];
	self.advanced = entry['info']['advanced'];
	self.default_value = ''
	self.type = 'string'
	if (entry['info']['default_value'])
		self.default_value = settings_get_value(entry['info']['default_value']);
	if (self.value() == self.default_value)
		self.value('')
	self.old_value = self.value()
	
	self.build_payload = function() {
		payload = {}
		payload['plugin_id'] = 1234
		payload['update'] = {}
		payload['update']['node'] = {}
		payload['update']['node']['path'] = self.path
		payload['update']['node']['key'] = self.key
		payload['update']['value'] = {}
		payload['update']['value']['string_data'] = self.value()
		return payload
	}
}

ko.extenders.scrollFollow = function (target, selector) {
	target.subscribe(function (newval) {
		var el = document.querySelector(selector);
		// the scroll bar is all the way down, so we know they want to follow the text
		if (el.scrollTop == el.scrollHeight - el.clientHeight) {
			// have to push our code outside of this thread since the text hasn't updated yet
			setTimeout(function () { el.scrollTop = el.scrollHeight - el.clientHeight; }, 0);
		}
	});
	
	return target;
};

ko.bindingHandlers.actionKey = {
    init: function(element, valueAccessor, allBindings) {
		var keys = allBindings.get('keys') || [13];
		if (!Array.isArray(keys))
			keys = [keys];

		$(element).keyup(function(event) {
			if (keys.indexOf(event.keyCode) > -1) {
				valueAccessor()(event);
			};
		});
    }
};
