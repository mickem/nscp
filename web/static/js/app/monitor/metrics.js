define(['knockout', 'app/core/utils', 'app/core/server', 'app/core/globalStatus'], function(ko, ut, server, gs) {

	function metrics() {
		var self = this;

		self.disks = {};
		self.diskIndex = 0;
		
		self.refresh_handler = false;

		self.cpu = {
			'load' : ko.observable('0%'),
			'handles' : ko.observable('0'),
			'threads' : ko.observable('0'),
			'procs' : ko.observable('0'),
			'uptime' : ko.observable('0'),
			'graph' : []
		}
		self.mem = {
			'pct' : ko.observable('0%'),
			'used' : ko.observable('0%'),
			'avail' : ko.observable('0'),
			'total' : ko.observable('0'),
			'unit' : ko.observable('0'),
			'commited' : ko.observable('0'),
			'virtual' : ko.observable('0'),
			'cache' : ko.observable('0'),
			'graph' : []
		}
		self.disk = ko.observableArray([])
		self.diskGraphs = {}
		
		self.set_refresh_handler = function(handler) {
			
			if (handler) {
				gs.add_refresh_handler('metrics', function() {
					server.json_get("/metrics", function(data) {

						var load = Math.round(data['system']['cpu']['total.total'])
						var steps = 10
						self.cpu.load(load + "%")
						
						self.cpu.handles(data['system']['metrics']['procs.handles'])
						self.cpu.threads(data['system']['metrics']['procs.threads'])
						self.cpu.procs(data['system']['metrics']['procs.procs'])
						self.cpu.uptime(data['system']['uptime']['uptime'])
						self.cpu.graph.push([ new Date().getTime(), load])
						if (self.cpu.graph.length > 120/5)
							self.cpu.graph.shift()

						
						var node = data['system']['metrics']
						var hasChanged = false;
						for (var key in node) {
							if (key.startsWith("pdh.disk_queue_length") && !key.endsWith("_Total")) {
								var id = key.split("_").last();
								if (id in self.disks) {
									index = self.disks[id];
								} else {
									index = self.diskIndex++;
									self.disks[id] = index;
								}
								
								var match = ko.utils.arrayFirst(self.disk(), function(item) {
									return id === item.id;
								});
								if (!match) {
									self.disk.push({ 'index': index, 'id': id, 'key': key, 'disk_queue_length': ko.observable(node[key])})
									self.diskGraphs[id] = [ new Date().getTime(), node[key] ]
									hasChanged = true;
								} else {
									match.disk_queue_length(node[key])
									self.diskGraphs[id].push([ new Date().getTime(), node[key] ])
									if (self.diskGraphs[id].length > 120/5)
										self.diskGraphs[id].shift()
								}
							}
						}

						var unit = ut.find_unit(data['system']['mem']['physical.total'])
						var pct = Math.round(data['system']['mem']['physical.used']*100/data['system']['mem']['physical.total'])
						self.mem.pct(pct)
						self.mem.avail(ut.scale_bytes(data['system']['mem']['physical.avail'], unit))
						self.mem.used(ut.scale_bytes(data['system']['mem']['physical.used'], unit))
						self.mem.total(ut.scale_bytes(data['system']['mem']['physical.total'], unit))
						self.mem.unit(unit)
						self.mem.commited(ut.scale_bytes(data['system']['mem']['commited.used'], unit))
						self.mem.virtual(ut.scale_bytes(data['system']['mem']['commited.total'], unit))
						self.mem.graph.push([ new Date().getTime(), pct])
						if (self.mem.graph.length > 120/5)
							self.mem.graph.shift()
						
						handler();
					})
				});
			} else {
				gs.remove_refresh_handler('metrics')
			}
		}
	};
	
	return new metrics();

});
