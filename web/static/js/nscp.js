function parseNagiosResult(id) {
	if (id == 0)
		return "OK"
	if (id == 1)
		return "WARNING"
	if (id == 2)
		return "CRITCAL"
	if (id == 3)
		return "UNKNOWN"
	return "INVALID(" + id + ")"
}
function parseNagiosResultCSS(id) {
	if (id == 0)
		return "btn-success"
	if (id == 1)
		return "btn-warning"
	if (id == 2)
		return "btn-danger"
	return "btn-info"
}

function getUrlVars() {
    var vars = [], hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for(var i = 0; i < hashes.length; i++)
    {
        hash = hashes[i].split('=');
        vars.push(hash[0]);
        vars[hash[0]] = hash[1];
    }
    return vars;
}