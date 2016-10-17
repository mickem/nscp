define(['jquery', 'knockout', 'text!app/index/dashboard.html', 
		'app/monitor/metrics', 'jquery.flot', 'jquery.flot.resize'], 
		function($, ko, templateString, metrics) {

	function updateGraph(plot, data, title, color) {
		var dataset = [ { label: title, data: data, color: "rgb(" + color + ")" } ]
		plot.setData(dataset)
		plot.draw()
	}
	function showGraph(elem, data, title, color) {
		var dataset = [ { label: title, data: data, color: "rgb(" + color + ")" } ]
		var options = {
			series: {
				lines: {
					show: true ,
					lineWidth: 1,
					fill: true,
					fillColor: 'rgba(' + color + ', 0.2)'
				},
				shadowSize: 0
			},
			xaxis: {
				//mode: "time",
				tickSize: [2, "second"],
				tickFormatter: function (v, axis) {
					var date = new Date(v);
					if (date.getSeconds() % 30 == 0) {
						var hours = date.getHours() < 10 ? "0" + date.getHours() : date.getHours();
						var minutes = date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes();
						var seconds = date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds();
						return hours + ":" + minutes + ":" + seconds;
					} else {
						return "";
					}
				},
				axisLabel: "Time",
				axisLabelUseCanvas: true,
				axisLabelFontSizePixels: 12,
				axisLabelFontFamily: 'Verdana, Arial',
				axisLabelPadding: 10
			},
			yaxis: {
				min: 0,
				max: 100,
				tickFormatter: function (v, axis) {
					if (v % 10 == 0) {
						return v + "%";
					} else {
						return "";
					}
				},
				axisLabel: "CPU load",
				axisLabelUseCanvas: true,
				axisLabelFontSizePixels: 12,
				axisLabelFontFamily: 'Verdana, Arial',
				axisLabelPadding: 6
			},
			legend: {
				labelBoxBorderColor: "#fff"
			},
			grid: {
				tickColor: "#D9EAF4"
			}
		}
		return $.plot(elem, dataset, options);
	}
	
	
	function DashboardViewmodel(params) {
		var self = this;
		
		if (params.tab && params.tab.startsWith('disk')) {
			self.diskIndex = params.tab.substr(4)
			self.tab = 'disk'
			console.log(params.tab)
			console.log(self.diskIndex)
		} else if (params.tab)
			self.tab = params.tab

		self.cpu = metrics.cpu
		if (self.cpu.data()) {
			if (!self.tab)
				self.tab = "cpu"
		}
		self.mem = metrics.mem
		if (self.mem.data()) {
			if (!self.tab)
				self.tab = "mem"
		}
		if (!self.tab)
			self.tab = "all"
		self.disk = metrics.disk
		self.diskGraphs = metrics.diskGraphs
		self.count = metrics.count
		self.metricsList = metrics.metricsList
		metrics.set_refresh_handler(function () {
			self.refreshChart();
		})
		self.dispose = function() {
			metrics.set_refresh_handler(false);
		}
		self.currentFilter = ko.observable();
		self.filterMetrics = ko.computed(function() {
			if(!self.currentFilter()) {
				return self.metricsList(); 
			} else {
				var key = self.currentFilter().toLowerCase()
				return ko.utils.arrayFilter(self.metricsList(), function(m) {
					return m.path.toLowerCase().indexOf(key) != -1;
				});
			}
		});
		


		self.refreshChart = function () {
			if (self.tab == 'cpu')
				showGraph('#cpuChart', self.cpu.graph, "% CPU Utilization", "76, 157, 203")
			else if (self.tab == 'mem')
				showGraph('#memChart', self.mem.graph, "Memory", "149, 40, 180")
			else if (self.tab == 'all')
				;
			else if (self.tab == 'disk') {
				if (self.diskIndex in self.disk()) {
					id = self.disk()[self.diskIndex].id
					showGraph('#diskChart', self.diskGraphs[id], "Disk " + id, "77, 166, 12")
				}
			} else {
				console.log("Unknown chart: " + self.tab)
			}
		}
		self.isDiskTab = function(index) {
			return self.tab == 'disk' && self.diskIndex == index;
		}
		
		$(document).ready(function(){
			self.refreshChart();
		});
	};

	return { 
		template: templateString, 
		viewModel: DashboardViewmodel
	};	
});
