define(['jquery', 'require', 'app/core/authToken', 'app/core/globalStatus'], function($, require, auth, globalStatus) {

	function gs() {
		return require("app/core/globalStatus");
	}

	return {
		json_get: function(url, success) {
			return $.ajax({
				url: url,
				type: 'GET',
				dataType: 'json',
				success: success,
				error: function(xhr, error, status) {
					if (xhr.status == 403) {
						gs().is_loggedin(false);
						auth.showLogin();
					}
					if (xhr.responseText)
						gs().set_error(xhr.responseText)
					else if (xhr.statusText)
						gs().set_error(xhr.statusText)
					gs().not_busy(false)
				},
				beforeSend: function (xhr) {
					xhr.setRequestHeader('TOKEN', auth.get());
				},
				timeout: 10000
			});
		},

		json_post: function(url, data, success) {
			return $.ajax({
				url: url,
				type: 'POST',
				dataType: 'json',
				data: data,
				success: success,
				error: function(xhr, error, status) {
					if (xhr.status == 403) {
						gs().is_loggedin(false);
						auth.showLogin();
					}
					gs().set_error(xhr.responseText)
					gs().not_busy(false)
				},
				beforeSend: function (xhr) {
					xhr.setRequestHeader('TOKEN', auth.get());
				}
			});
		}
	};
});
