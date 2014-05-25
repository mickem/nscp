function PathNode(parent, path) {
	var self = this;
	self.href = "/settings.html?path=" + parent + "/" + path
	self.title = "Show all keys under: " + parent + "/" + path
	self.name = path
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

function PathEntry(path, desc, plugs) {
	var self = this;
	self.path = path;
	self.paths = make_paths_from_string(path)
	self.desc = desc;
	self.plugs = plugs;
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}


function KeyEntry(path, key, desc, plugs) {
	var self = this;
	self.path = path;
	self.key = key;
	self.value = ''
	self.paths = make_paths_from_string(path)
	self.desc = desc;
	self.plugs = plugs;
	self.showDetails = ko.observable(false);
	
	self.showMore = function() {
		self.showDetails(!self.showDetails());
	}
}
function CommandViewModel() {
	var self = this;

	path = getUrlVars()['path']
	if (!path)
		path = '/'
	self.currentPath = ko.observableArray(make_paths_from_string(path))
	self.paths = ko.observableArray([]);
	self.keys = ko.observableArray([]);
	self.result = ko.observable("TODO");
	self.resultCSS = ko.computed(function() {
		return parseNagiosResultCSS(this.result().result);
		}, self);

	self.execute = function(command) {
		$("#result").modal('show');
		
		$.getJSON("/settings/" + command.name, function(data) {
			data['payload'].resultText = parseNagiosResult(data['payload'].result)
			self.result(data['payload'])
			//data['payload']['inventory'].forEach(function(entry) {
			//	self.paths.push(new PathEntry(entry['name'], entry['info']['description'], entry['info']['plugin']));
			//});
		})
		
	}
	self.load = function() {
		$.getJSON("/settings/inventory?path=" + path + "&recursive=true&paths=true", function(data) {
			self.paths.removeAll()
			data['payload'][0]['inventory'].forEach(function(entry) {
				self.paths.push(new PathEntry(entry['node']['path'], entry['info']['description'], entry['info']['plugin']));
			});
		})
		self.keys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
		$.getJSON("/settings/inventory?path=" + path + "&recursive=false&keys=true", function(data) {
			self.keys.removeAll()
			data['payload'][0]['inventory'].forEach(function(entry) {
				console.log(entry)
				self.keys.push(new KeyEntry(entry['node']['path'], entry['node']['key'], entry['info']['description'], entry['info']['plugin']));
			});
		})
		self.keys.sort(function(left, right) { return left.name == right.name ? 0 : (left.name < right.name ? -1 : 1) })
	}
	self.load()
}
ko.applyBindings(new CommandViewModel());