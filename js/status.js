function tobytes(s) {
	var b = [];
	for (var i = 0; i < s.length; i++) {
		b.push(s[i].charCodeAt());
	}
	return b;
}

var serverpass = "";
var id = (new Date().getTime()) & 0xFFFF;	

function getstatus() {
	var d = '{"jsonrpc":"1.0","method":"getblockchaininfo","params":[],"id":' + id.toString() + '}';
	id++;

	var auth = Crypto.util.bytesToBase64(tobytes("admin:" + serverpass));

	$.ajax({
		url:'http://127.0.0.1:8789',
		type: "POST",	// different version of jquery has different name for this field
		data:d,
		global: true,
		beforeSend: function(xhr) { 
			xhr.setRequestHeader("Authorization", "Basic " + auth);
		},
		dataType:"json",
		success: function (resp) {
			if (resp.error) return;
			window.chrome.webview.postMessage("StatusUpdate " + resp.result.mediantime.toString());				
		},
	});
}

setInterval("getstatus()", 30000);
