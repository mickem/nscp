function parseNagiosResult(id) {
	if (id == 0)
		return "OK"
	if (id == 1)
		return "WARNING"
	if (id == 2)
		return "CRITICAL"
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
		json_get("/settings/status", function(data) {
			console.log(data)
			self.update(data['payload'][0]['status'])
		})
	}	
}

function auth_token_class() {
	var self = this
	self.token = global_token
	self.get = function() {
		return self.token;
	}
	self.set = function(token) {
		self.token = token;
		//window.localStorage.setItem('access_token', token);
	}
}
var auth_token = new auth_token_class()

function NSCPStatus(state) {
	var self = this;
	self.poller_state = typeof state !== 'undefined' ? state : true;
	self.has_issues = ko.observable('')
	self.is_loggedin = ko.observable(auth_token.get()!='')
	self.error_count = ko.observable('')
	self.last_error = ko.observable('')
	self.busy_header = ko.observable('')
	self.busy_text = ko.observable('')
	self.settings = ko.observable(new SettingsStatus());
	self.password = ko.observable('');
	self.count = 0;
	
	self.poller = null;
	self.on_login = function() {}
	self.set_on_login = function(f) { self.on_login = f} 
	self.on_logout = function() {}
	self.set_on_logout = function(f) { self.on_logout = f} 
	self.login_error_message = ko.observable('')
	
	self.login = function() {
		$.getJSON("/auth/token?password="+self.password(), function(data, textStatus, xhr) {
			auth_token.set(xhr.getResponseHeader("__TOKEN"));
			href = window.location.href
			pos = href.indexOf('?__TOKEN')
			if (pos != -1)
				href =  href.substr(0, pos)
			href = href + '?__TOKEN='+auth_token.get()
			window.location.href = href;
			self.password('')
			self.is_loggedin(true)
			self.on_login()
		}).error(function(xhr, error, status) {
		console.log(error)
			self.login_error_message(xhr.responseText)
		})

	}
	self.logout = function() {
		$.getJSON("/auth/logout?token="+auth_token.get(), function(data, textStatus, xhr) {
			auth_token.set('');
			self.password('')
			self.is_loggedin(false)
			self.on_logout()
		})
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
		console.log("Error: " + text)
		self.error_count(self.error_count()+1)
		self.last_error(text)
		self.has_issues(self.error_count() > 0)
	}
	self.busy = function(header, text, count) {
		console.log("++busy: " + count)
		if (self.count < 0)
			self.count = 0
		count = typeof count !== 'undefined' ? count : 1;
		self.count = count
		console.log("::busy: " + self.count)
		if (self.poller_state)
			self.cancelPoller();
		self.busy_header(header)
		self.busy_text(text)
		$('#busy').modal();
	}
	self.not_busy = function(count, start_poller) {
		console.log("--busy: " + count)
		start_poller = typeof start_poller !== 'undefined' ? start_poller : true;
		count = typeof count !== 'undefined' ? count : 1;
		self.count -= count
		console.log("::busy: " + self.count + ", " + start_poller)
		if (self.count <= 0) {
			$('#busy').modal('hide');
			self.busy_header('')
			self.busy_text('')
			console.log("::HIT ME: " + self.poller_state + ", " + start_poller)
			if (self.poller_state && start_poller)
				self.start();
		}
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
		
	self.do_update = function(elem) {
		json_get("/log/status", function(data) {
		console.log(data)
			self.update(data['status']);
		}).error(function(xhr, error, status) {
			if (xhr.status == 200)
				return;
			console.log("Status got error: " + xhr.status);
			if (xhr.status == 403) {
				global_status.is_loggedin(false);
			}
			global_status.set_error(xhr.responseText)
			global_status.not_busy(100, false)
		})
		self.settings_refresh()
	}
	self.poll = function() {
		if (self.is_loggedin()) {
			self.do_update();
		}
		self.poller = setTimeout(self.poll, 5000);
	}

	self.reset = function() {
		json_get("/log/reset", function(data) {
			self.do_update();
		})
	}
	self.cancelPoller = function() {
		console.log("Stopping poller")
		clearTimeout(self.poller);
	}
	self.schedule_restart_poll = function() {
		console.log("Starting  poller")
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
		json_get("/core/reload", function(data) {
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
}


var global_status = new NSCPStatus();


function json_get(url, success) {
	return $.ajax({
		url: url,
		type: 'GET',
		dataType: 'json',
		success: success,
		error: function(xhr, error, status) {
			if (xhr.status == 403) {
				global_status.is_loggedin(false);
			}
			global_status.set_error(xhr.responseText)
			global_status.not_busy(100, false)
		},
		beforeSend: function (xhr) {
			xhr.setRequestHeader('TOKEN', auth_token.get());
		}
	});
}

function json_post(url, data, success) {
	return $.ajax({
		url: url,
		type: 'POST',
		dataType: 'json',
		data: data,
		success: success,
		error: function(xhr, error, status) {
			if (xhr.status == 403) {
				global_status.is_loggedin(false);
			}
			global_status.set_error(xhr.responseText)
			global_status.not_busy(100, false)
		},
		beforeSend: function (xhr) {
			xhr.setRequestHeader('TOKEN', auth_token.get());
		}
	});
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
	if (entry['info']) {
		info = entry['info']
		if (info['title'])
			self.title = info['title'];
		else
			self.title = 'UNDEFINED KEY'
		if (info['description'])
			self.desc = info['description'];
		else
			self.desc = 'UNDEFINED KEY'
		self.plugs = info['plugin'];
		self.advanced = info['advanced'];
		if (info['default_value'])
			self.default_value = settings_get_value(info['default_value']);
	}
	self.default_value = ''
	self.type = 'string'
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

ko.bindingHandlers.expand = {
    init: function (element, valueAccessor, allBindingsAccessor, viewModel) {
        if ($(element).hasClass('ui-expander')) {
            var expander = element;
            var head = $(expander).find('.ui-expander-head');
            var content = $(expander).find('.ui-expander-content');
        
            $(head).click(function () {
                      $(head).toggleClass('ui-expander-head-collapsed');
                      $(content).toggle();
            });
        }
    }
};

ko.bindingHandlers.select2 = {
	init: function(element, valueAccessor, allBindings, viewModel, bindingContext) {
		ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
			//$(element).select2('destroy');
		});
		
		select2 = ko.utils.unwrapObservable(allBindings().select2);
		$(element).select2(select2);
	},
	update: function(element, valueAccessor, allBindings, viewModel, bindingContext) {
		/*
		$(element).select2({
			formatResult: format,
			formatSelection: format,
			placeholder: "Add argument",
			escapeMarkup: function(m) { return m; }
		});
		*/
	}
};
/*
ko.bindingHandlers.typeahead = {
	init: function (element, valueAccessor, allBindings, viewModel, bindingContext) {
		var $element = $(element);
		var typeaheadOpts = { source: ko.utils.unwrapObservable(valueAccessor()) };

		if (allBindings().typeaheadOptions) {
			$.each(allBindings().typeaheadOptions, function(optionName, optionValue) {
				typeaheadOpts[optionName] = ko.utils.unwrapObservable(optionValue);
			});
		}
		console.log(typeaheadOpts)
		console.log($element)
		console.log($element.attr)
		console.log($element.attr("autocomplete", "off"))
		$element.attr("autocomplete", "off").typeahead(typeaheadOpts);
	}
};
*/

var substringMatcher = function(strs) {
  return function findMatches(q, cb) {
	console.log("-----------")
    var matches, substrRegex;
 
    // an array that will be populated with substring matches
    matches = [];
 
    // regex used to determine if a string contains the substring `q`
    substrRegex = new RegExp(q, 'i');
 
    // iterate through the pool of strings and for any string that
    // contains the substring `q`, add it to the `matches` array
    $.each(strs, function(i, str) {
      if (substrRegex.test(str)) {
        // the typeahead jQuery plugin expects suggestions to a
        // JavaScript object, refer to typeahead docs for more info
        matches.push({ value: str });
      }
    });
 
    cb(matches);
  };
};

ko.bindingHandlers.typeahead = {
    init: function(element, valueAccessor) {
        var options = ko.unwrap(valueAccessor()) || {},
            $el = $(element),
            triggerChange = function() {
                $el.change();   
            }

        //initialize widget and ensure that change event is triggered when updated
		$el.typeahead({
		  hint: true,
		  highlight: true,
		  minLength: 1
		},options)
            .on("typeahead:selected", triggerChange)
            .on("typeahead:autocompleted", triggerChange);        
			

        //if KO removes the element via templating, then destroy the typeahead
        ko.utils.domNodeDisposal.addDisposeCallback(element, function() {
            $el.typeahead("destroy"); 
            $el = null;
        }); 
    }
};
