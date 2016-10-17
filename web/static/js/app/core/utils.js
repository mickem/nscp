define(['knockout'], function(ko) {
	
	
	function next_unit(unit) {
		if (unit == '')
			return 'b'
		if (unit == 'b')
			return 'kb'
		if (unit == 'kb')
			return 'mb'
		if (unit == 'mb')
			return 'gb'
		if (unit == 'gb')
			return 'pb'
	}
	return {
		find_unit: function(value) {
			unit = 'b'
			while (value > 1023 && unit != 'pb') {
				value /= 1024;
				unit = next_unit(unit);
			}
			return unit;
		},
		scale_bytes: function(value, unit) {
			if (unit == 'b')
				return Math.round(value*10)/10;
			if (unit == 'kb')
				return Math.round(value/1024*10)/10;
			if (unit == 'mb')
				return Math.round(value/1024/1024*10)/10;
			if (unit == 'gb')
				return Math.round(value/1024/1024/1024*10)/10;
			if (unit == 'pb')
				return Math.round(value/1024/1024/1022/1024*10)/10;
			return value
		},
		parseNagiosResult: function(id) {
			if (id == 0)
				return "OK"
			if (id == 1)
				return "WARNING"
			if (id == 2)
				return "CRITICAL"
			if (id == 3)
				return "UNKNOWN"
			return "INVALID(" + id + ")"
		},
		parseNagiosResultCSS: function(id) {
			if (id == 0)
				return "btn-success"
			if (id == 1)
				return "btn-warning"
			if (id == 2)
				return "btn-danger"
			return "btn-info"
		},
		getUrlVars: function() {
			var vars = [], hash;
			var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
			for(var i = 0; i < hashes.length; i++)
			{
				hash = hashes[i].split('=');
				vars.push(hash[0]);
				vars[hash[0]] = hash[1];
			}
			return vars;
		},
		settings_get_value: function (val) {
			if (val.string_data)
				return val.string_data
		},
		groupBy: function(array, f) {
			var groups = {};
			array.forEach( function(o) {
				var group = JSON.stringify( f(o) );
				groups[group] = groups[group] || [];
				groups[group].push( o );  
			});
			return Object.keys(groups).map( function( group ) {
				return groups[group]; 
			})
		},
		toggleChevron: function(e) {
			$(e.target)
				.prev('.panel-heading')
				.find("i.indicator")
				.toggleClass('glyphicon-chevron-down glyphicon-chevron-up');
		},
		convertToObservable: function(list) { 
			var newList = []; 
			$.each(list, function (i, obj) {
				var newObj = {}; 
				Object.keys(obj).forEach(function (key) { 
					newObj[key] = ko.observable(obj[key]); 
				}); 
				newList.push(newObj); 
			}); 
			return newList; 
		},
		substringMatcher: function(strs) {
			return function findMatches(q, cb) {
				var matches, substrRegex;
				matches = [];
				substrRegex = new RegExp(q, 'i');
				$.each(strs, function(i, str) {
					if (substrRegex.test(str)) {
						matches.push({ value: str });
					}
				});
				cb(matches);
			};
		},
		parseExecCommand: function(cmd) {
			args = cmd.match(/[\w=-]+|"(?:\\"|[^"]|[=-])+"/g);
			if (args.length < 2)
				return ""
			str = args[0] + "/" + args[1]
			
			for (var i = 2; i < args.length; i++) {
				var sep = "&"
				if (i == 2)
					sep = "?"
				var s = args[i]
				if (s.substring(0, 1) === '"' && s.substr(s.length-1) === '"')
					s = s.substring(1, s.length-1)
				as = s.split('=', 2);
				if (as.length > 1) {
					str = str + sep +  encodeURIComponent(as[0]) + "=" + encodeURIComponent(as[1])
				} else {
					str = str + sep +  encodeURIComponent(s)
				}
			}
			return str
		},
		parseCommand: function(cmd) {
			args = cmd.match(/[\w=-]+|"(?:\\"|[^"]|[=-])+"/g);
			str = args[0]
			
			for (var i = 1; i < args.length; i++) {
				var sep = "&"
				if (i == 0)
					sep = ""
				if (i == 1)
					sep = "?"
				var s = args[i]
				if (s.substring(0, 1) === '"' && s.substr(s.length-1) === '"')
					s = s.substring(1, s.length-1)
				as = s.split('=', 2);
				if (as.length > 1) {
					str = str + sep +  encodeURIComponent(as[0]) + "=" + encodeURIComponent(as[1])
				} else {
					str = str + sep +  encodeURIComponent(s)
				}
			}
			return str
		}

	};
});
