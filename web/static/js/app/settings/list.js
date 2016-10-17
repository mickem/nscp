define(['knockout', 'text!app/settings/list.html', 'app/core/settings', 'bootstrap-treeview'], 
	function(ko, templateString, settings, bst) {
	var nodeMap = {}
	function build_tree(n, tgt) {
		var node = {
			text: n.name,
			source: n,
			path: n.path,
			href: '#/settings' + n.path,
			state: {}
		}
		if (n.children && n.children.length > 0) {
			var nodes = []
			for (var i=0; i<n.children.length; i++) {
				var cnode = build_tree(n.children[i], tgt)
				if (cnode['state']['expanded'])
					node['state']['expanded'] = true;
				nodes.push(cnode)
			}
			node['nodes'] = nodes
		}
		if (n.path == tgt.path) {
			node['state']['selected'] = true;
			node['state']['expanded'] = true;
			tgt.keys(node.source.keys)
			tgt.akeys(node.source.akeys)
			ckeys = []
			node.source.keys.forEach(function (e) {
				if (!e.is_default()) {
					ckeys.push(e);
				}
			})
			node.source.akeys.forEach(function (e) {
				if (!e.is_default()) {
					ckeys.push(e);
				}
			})
			tgt.ckeys(ckeys)
			tgt.current(node.source)
		}
		nodeMap[n.path] = node
		return node
	}

	function SettingsViewModel(param) {
		var self = this;
		
		self.keys = ko.observableArray([])
		self.akeys = ko.observableArray([])
		self.ckeys = ko.observableArray([])
		self.current = ko.observable()
		self.tpls = ko.observableArray([])

		self.newPath = ko.observable()
		self.newKey = ko.observable()
		self.newValue = ko.observable()
		
		self.path = "/modules"
		if (param && param.splat && param.splat.length > 0) {
			self.path = param.splat[0]
		}
		if (self.path == "")
			self.path = "/modules"
		self.newPath(self.path)
		self.refresh = function() {
			settings.refresh(function (data) {
				self.update_tree();
			})
		}
		self.add = function() {
			settings.add(self.newPath(), self.newKey(), self.newValue(), function () {
				self.update_tree();
			});
		}
		self.save = function() {
			settings.save(function () {
				self.update_tree();
			});
		}
		self.undo = function() {
			settings.undo();
		}
		self.update_tree = function() {
			settings.get("/", function(root) {
				settings.get_templates(self.path, function(tpls) {self.tpls(tpls)})
				stree = build_tree(root, self)
				$('#tree').treeview({
						data: stree.nodes, 
						onNodeSelected: function(event, data) {
							window.location = data.href
						}
					});
			})
		}
		self.update_tree()
	}
	return { 
		template: templateString, 
		viewModel: SettingsViewModel 
	};	
})

