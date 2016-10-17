require.config({
  baseUrl: 'static/js/lib',
  paths: {
    app: '../app',
    'text': 'require-text-2.0.14',
    'knockout': 'knockout-3.3.0',
    'bootstrap': 'bootstrap.min',
    'select2': 'select2.min',
    'bootstrap-dialog': 'bootstrap-dialog.min',
    'bootstrap-treeview': 'bootstrap-treeview.min',
    'jquery': 'jquery-1.11.1.min',
    'jquery.flot': 'jquery.flot.min',
    'typeahead.jquery': 'typeahead.jquery.min',
    'jquery.flot.resize': 'jquery.flot.resize.min',
    'handlebars' : 'handlebars-v2.0.0'
  }
  ,shim: {
    'select2': {
      deps: ['jquery'],
      exports: 'Select2'
    },
    'bootstrap': {
      deps: ['jquery'],
      exports: '$.fn.popover'
    },
    'jquery.flot.resize': {
      deps: ['jquery.flot']
    },
    'jquery': {
      exports: '$'
    }
  }
});


if (typeof String.prototype.startsWith != 'function') {
	String.prototype.startsWith = function (str){
		return this.indexOf(str) === 0;
	};
}
String.prototype.endsWith = function(suffix) {
	return this.indexOf(suffix, this.length - suffix.length) !== -1;
};
if (!Array.prototype.last){
	Array.prototype.last = function(){
		return this[this.length - 1];
	};
};
if (typeof String.prototype.replaceAll != 'function') {
	String.prototype.replaceAll = function (find, replace) {
		var str = this;
		return str.replace(new RegExp(find.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&'), 'g'), replace);
	};
}
define(['knockout', 'bootstrap', 'app/core/globalStatus', 'sammy', 'app/core/authToken', 'app/core/settings', 'select2'], function(ko, b, gs, Sammy, auth, settings) {

	ko.components.register('dash-widget-text', {
		viewModel: function(params) { 
			this.title = params.title;
			this.value = params.value;
			this.iconClass = params.icon;
			if (params.width)
				this.width = 'col-lg-' + params.width;
			else {
				this.width = 'col-lg-3';
			}
			if (params.background)
				this.background = 'panel-' + params.background;
			else {
				this.background = 'panel-info';
			}
		},
		template: { require: 'text!app/tpl/dash-widget-text.html' }
	});
	ko.components.register('settings-list-wdgt', {
		viewModel: function(params) { 
			this.keys = params.keys;
		},
		template: { require: 'text!app/tpl/settings-list.html' }
	});
	
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
		init: function(element, valueAccessor, allBindings, data, context) {
			var options = ko.toJS(valueAccessor()) || {};
			setTimeout(function() {
				$(element).select2(options);            
			}, 0);
		}
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
	
	var pageVm = {
		name: ko.observable(),
		tab: ko.observable(),
		data: ko.observable(),
		nscp_status: gs,
		settings: settings,
		setRoute: function(name, tab, data) {
			//console.log(name)
			//console.log(tab)
			//console.log(data)
			this.name(name);
			this.tab(tab);
			this.data(data);
		},
		getOld: function() {
			return {
				name: this.name(),
				tab: this.tab(),
				data: this.data()
			} 
		},
		restoreOld: function(old) {
			this.name(old.name);
			this.tab(old.tab);
			this.data(old.data); 
		}
	};
	
	var sammyConfig = Sammy('#appHost', function() {
		var self = this;
		
		// Override this function so that Sammy doesn't mess with forms
		this._checkFormSubmission = function(form) {
			return (false);
		};
		
		var pages = [
			{ route: ['/', '#/', '#/metrics/:tab'],
				tab: 'index',
				component: 'metrics-dashboard',
				module: 'app/index/dashboard'},
			{ route: ['#/settings(.*)$', '#/settings(.*)/$'], 
				tab: 'settings',
				component: 'settings-list', 
				module: 'app/settings/list' },
			{ route: ['#/templates(.*)$', '#/templates(.*)/$'], 
				tab: 'settings',
				component: 'settings-tpl', 
				module: 'app/settings/tpl' },
			{ route: '#/modules', 
				tab: 'modules',
				component: 'modules-list', 
				module: 'app/modules/list' },
			{ route: '#/modules/:id', 
				tab: 'modules',
				component: 'modules-page', 
				module: 'app/modules/module' },
			{ route: '#/queries', 
				tab: 'queries',
				component: 'queries-list', 
				module: 'app/queries/list' },
			{ route: '#/queries/:id', 
				tab: 'queries',
				component: 'queries-page', 
				module: 'app/queries/query' },
			{ route: '#/log', 
				tab: 'log',
				component: 'log-view', 
				module: 'app/log/log' },
			{ route: '#/console', 
				tab: 'console',
				component: 'console-view', 
				module: 'app/log/console' },
			{ route: '#/login', 
				tab: 'index',
				component: 'login-page', 
				module: 'app/misc/login' }
		];

		pages.forEach(function(page) {
			ko.components.register(page.component, { require: page.module });
			if (!(page.route instanceof Array))
				page.route = [page.route];

			page.route.forEach(function(route) {
				self.get(route, function() {
					var params = {};
					ko.utils.objectForEach(this.params, function(name, value) {
						if (name == "__TOKEN") {
							auth.set(value)
							gs.is_loggedin(true);
						}
						params[name] = value;
					});
					pageVm.setRoute(page.component, page.tab, params);
				});
			});
		});
	});

	$(document).ready(function() {
		auth.register(pageVm)
		sammyConfig.run('#/');
		if (!gs.is_loggedin()) {
			auth.showLogin()
		}
		ko.applyBindings(pageVm);
	});

});

