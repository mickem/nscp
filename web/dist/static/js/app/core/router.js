define(['app/core/authToken'], function(auth) {
	return {
		navigate: function(path) {
			window.location.hash = '#' + path + '?__TOKEN='+auth.get()
		}
	};
});