define(['jquery', 'require', 'app/core/authToken', 'app/core/globalStatus'], function($, require, auth, globalStatus) {

	function gs() {
		return require("core/globalStatus");
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
					gs().set_error(xhr.responseText)
					gs().not_busy(100, false)
				},
				beforeSend: function (xhr) {
					xhr.setRequestHeader('TOKEN', auth.get());
				}
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
					gs().not_busy(100, false)
				},
				beforeSend: function (xhr) {
					xhr.setRequestHeader('TOKEN', auth.get());
				}
			});
		}
	};
});
