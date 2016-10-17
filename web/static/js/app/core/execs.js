define(['knockout', 'app/core/server'], function(ko, server) {

	function executeExec(query, cb) {
		server.json_get("/exec/" + query, function(data) {
			if (data['payload'] && data['payload'].length > 0) {
				r = data['payload'][0]
				cb(r.result, r.message)
			} else {
				cb(1, "")
			}
		})
	}

	function execs() {
		var self = this;
		self.execute = executeExec
	}
	return new execs;
})