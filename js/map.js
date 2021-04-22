	var link;
	var map;
	var north, south, east, west, type, zoom = 1;
	var extent = {east: -1, west:1, south:1, north:-1 };
	var markerextent = {east: -1, west:1, south:1, north:-1 };
	var infowindow = null;
	var lat = 0, lng = 0;
	var tries = 1;
	var bound = null;
	var minw = -1.0;

	var total = [];
	var loop = [];
	var polygon = [];
	var outrights = [];
	var seq = 1;
	var blockstack = [];

	var drawnpolygons = [];
	var polygons = [];
	var vertexmode = false;

	var landvalue = 0.0;
	var landtotake = 0;
	var allowquick = true;

	var ins = ['vertex', 'border', 'polygon', 'input', 'spend'];

	// alt: var ins = ['vertex', 'border', 'polygon', 'right', 'input', 'output', 'output2', 'output3'];

	function showedges(app) {
		var bound = getBounds();
		lod = Math.ceil(Math.log(bound.east - bound.west) / Math.log(2.0) + 14);
		url = './index.php?module=land&MOD_op=findborder&west=' + bound.west + '&east=' + bound.east + '&south=' + bound.south + '&north=' + bound.north + '&lod=' + lod + '&app=' + app;

		$.ajax({
			url: url,
			type:'post',
			dataType: 'json',
			success: function(resp) {
				for (var i = 0; i < resp.length; i++) {
					drawBorder(resp[i]);
				}
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
	}

	function pop(s) {
		for (var i = 0; i < ins.length; i++) {
			var el = document.getElementById(ins[i]);
			if (el) el.style.display = 'none';
		}
		document.getElementById(s).style.display = 'block';
		vertexmode = (s == 'vertex');
	}

	function withdraw() {
		if (blockstack.length > 0) {
			if (blockstack[blockstack.length-1].block != null)
				document.getElementById(blockstack[blockstack.length-1].block).style.display = 'none';
			switch (blockstack[blockstack.length-1].type) {
				case 0: var m = blockstack[blockstack.length-1].marker;
					if (m) m.setMap(null);
					total.length--;
					break;
				case 1: loop.length--;
					break;
				case 2: outrights.length--;
					break;
				case 3: loop = polygon[polygon.length - 1];
					polygon.length--;
					break;
				case 4: polygon = total[total.length - 1].polygon;
					total.length--;
					break;
			}
			blockstack.length--;
		}
	}

	function addvertex(cont) {
		var flat = document.getElementById('lat').value;
		var flng = document.getElementById('lng').value;
		var lat = parseInt(flat * 2097152);
		var lng = parseInt(flng * 2097152);

		pointDropEnd();

		if (flat !='' && flng != '') {
			total.push({'type':'vertex', 'lng':lng, 'lat':lat, 'seq':seq});
			blockstack.push({'block':'block' + seq, 'type':0, 'marker':setMarker(map, parseFloat(flat), parseFloat(flng), total.length)});

			document.getElementById('params').innerHTML += '<div id=block' + seq + '>顶点[' + total.length + ']：' + flng + ', ' + flat + '<br></div>';
			seq++;
		}

		if (!cont) {
			document.getElementById('vertex').style.display = 'none';
			vertexmode = false;

			if (allowquick)
				document.getElementById('params').innerHTML += '<div><a href=# onclick=quickpolygon(false);>Quick creation (Counter Clockwise)</a><br><a href=# onclick=quickpolygon(true);>Quick creation (Clockwise)</a></div>';
		}
	}
	
	function quickpolygon(cw) {
		if (!allowquick) {
			alert("Not allowed.");
			return;
		}

		allowquick = false;

		var points = [];
		for (var i = 0; i < total.length; i++) {
			if (total[i].type == 'vertex') {
				points.push(i + 1);
			}
		}

		if (points.length < 2) {
			alert('至少需要2个顶点');
			return;
		}

		points.push(points[0]);

		end = points[0];
		v = [{lat: total[end - 1].lat / 2097152, lng: total[end - 1].lng / 2097152}];

		var loop = [];

		for (var i = 1; i < points.length; i++) {
			begin = end; end = points[i];			
			v.push({lat: total[end - 1].lat / 2097152, lng: total[end - 1].lng / 2097152});
			
			total.push({'type':'border', 'father':father, 'begin':begin, 'end':end, 'seq':seq});
			blockstack.push({'block':'block' + seq, 'type':0});

			document.getElementById('params').innerHTML += '<div id=block' + seq + '>边界线[' + total.length + ']：父边界线=' + father + '<br>(' + begin + ', ' + end + ')</div>';
			seq++;

			loop.push({'border':total.length, 'reversed':cw, 'seq':seq});
			seq++;
		}


		drawEdge(v, "#00FF00");

		if (cw) loop.reverse();

		polygon.push(loop);
		loop = [];
		blockstack.push({'block':null, 'type':3});

		total.push({'type':'polygon', 'polygon':polygon, 'seq':seq});
		document.getElementById('params').innerHTML += '<div id=block' + seq + '>多边形[' + total.length + ']：' + JSON.stringify(polygon) + '<br>';
		blockstack.push({'block':'block' + seq, 'type':4});

		document.getElementById('polygon').style.display = 'none';

		url = './index.php?module=land&MOD_op=findvalue&location=' + landlocation + '&polygon=' + seq + '&data=' + JSON.stringify(total);

		landtotake = total.length;

		seq++;

		document.getElementById('editland').style.display = 'none';

		$.ajax({
			url: url,
			type:'post',
			dataType: 'json',
			success: function(resp) {
				if (resp.value == 0) {
					alert("无法计算这块土地的价值，可能的原因是：" + resp.reason);
				} else {
//					alert("这块土地的价值是" + resp.value + " BOEs");
					landvalue = resp.value;
					document.getElementById('params').innerHTML += '<div>Land price: ' + resp.value + " BOEs</div>";
//					document.getElementById('payment').value = landvalue;
//					pop("spend");
				}
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
		polygon = [];
		submitrawlandtx();
	}

	function addborder() {
		var father = document.getElementById('father').value;
		var points = document.getElementById('points').value.split(',');

		if (points.length < 2) {
			alert('至少需要2个顶点');
			return;
		}

		end = points[0];
		v = [{lat: total[end - 1].lat / 2097152, lng: total[end - 1].lng / 2097152}];

		for (var i = 1; i < points.length; i++) {
			begin = end; end = points[i];
			v.push({lat: total[end - 1].lat / 2097152, lng: total[end - 1].lng / 2097152});
			
			total.push({'type':'border', 'father':father, 'begin':begin, 'end':end, 'seq':seq});
			blockstack.push({'block':'block' + seq, 'type':0});

			document.getElementById('params').innerHTML += '<div id=block' + seq + '>边界线[' + total.length + ']：父边界线=' + father + '<br>(' + begin + ', ' + end + ')</div>';
			seq++;
		}

		drawEdge(v, "#00FF00");

		allowquick = false;

		document.getElementById('border').style.display = 'none';
	}

	function addborder2() {
		var border = document.getElementById('bordedr').value;
		var reversed = document.getElementById('reversdir').checked;
			
		total.push({'type':'border', 'border':border, 'seq':seq});
		blockstack.push({'block':'block' + seq, 'type':0});

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>边界线[' + total.length + ']：父边界线=' + father + '<br>(' + begin + ', ' + border + ')<br></div>';
		seq++;

		allowquick = false;

		document.getElementById('border').style.display = 'none';
	}
	
	function addLoop() {
		var lps = document.getElementById('loop').value.split(',');
		var reversed = document.getElementById('reversed').checked;

		if (lps.length < 2) {
			return false;
		}

		for (var i = 0; i < lps.length; i++) {
			lp = lps[i];
			loop.push({'border':lp, 'reversed':reversed, 'seq':seq});
			blockstack.push({'block':'block' + seq, 'type':1});

			document.getElementById('params').innerHTML += '<div id=block' + seq + '>环[' + (polygon.length + 1) + ']+：' + lp + ', ' + reversed + '<br></div>';
			seq++;
		}

		allowquick = false;

		document.getElementById('polygon').style.display = 'none';
		document.getElementById('loop').value = '';
		return true;
	}

	function addpolygon(close) {
		allowquick = false;

		addLoop();

		polygon.push(loop);
		loop = [];
		blockstack.push({'block':null, 'type':3});

		if (close) {
			total.push({'type':'polygon', 'polygon':polygon, 'seq':seq});
			document.getElementById('params').innerHTML += '<div id=block' + seq + '>多边形[' + total.length + ']：' + JSON.stringify(polygon) + '<br>';
			blockstack.push({'block':'block' + seq, 'type':4});
			seq++;
			polygon = [];
		}

		document.getElementById('polygon').style.display = 'none';
	}

	function addlandpolygon() {
		var url;
		var obj = document.getElementById('location');

		allowquick = false;

		if (addLoop()) {
			polygon.push(loop);
			loop = [];
			blockstack.push({'block':null, 'type':3});

			total.push({'type':'polygon', 'polygon':polygon, 'seq':seq});
			document.getElementById('params').innerHTML += '<div id=block' + seq + '>多边形[' + total.length + ']：' + JSON.stringify(polygon) + '<br>';
			blockstack.push({'block':'block' + seq, 'type':4});

			document.getElementById('polygon').style.display = 'none';

			url = './index.php?module=land&MOD_op=findvalue&location=' + landlocation + '&polygon=' + seq + '&data=' + JSON.stringify(total);

			landtotake = total.length;

			seq++;
		} else if (landtotake > 0) {
			url = './index.php?module=land&MOD_op=findvalue&location=' + landlocation + '&polygon=' + landtotake + '&data=' + JSON.stringify(total);
		} else return;

		document.getElementById('editland').style.display = 'none';

		$.ajax({
			url: url,
			type:'post',
			dataType: 'json',
			success: function(resp) {
				if (resp.value == 0) {
					alert("无法计算这块土地的价值，可能的原因是：" + resp.reason);
				} else {
					alert("这块土地的价值是" + resp.value + " BOEs");
					landvalue = resp.value;
					document.getElementById('params').innerHTML += '<div>Land price: ' + resp.value + " BOEs</div>";
//					document.getElementById('payment').value = landvalue;
//					pop("spend");
				}
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
		polygon = [];
	}

	function addright(s) {
		var father = document.getElementById('fatherRight').value;
		var desc = document.getElementById('content').value;
		var affirm = document.getElementById('affirm').checked;
		var divisible = document.getElementById('divisible').checked;
		total.push({'type':'right', 'father':father, 'desc':desc, 'affirm':affirm?1:0, 'divisible':divisible?1:0, 'seq':seq});

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>权利：父权益：' + father + (affirm?'否定' : '') + ', ' + (divisible?'可分' : '') + '</div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('right').style.display = 'none';
	}

	function addinput(s) {
		var txin = document.getElementById('txin').value;
		var txseq = document.getElementById('txseq').value;
		total.push({'type':'input', 'txin':txin, 'txseq':txseq, 'seq':seq});

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>输入[' + total.length + ']：<input type=text id=txin[' + seq + '] value=' + txin + '>, <input type=text id=txseq[' + seq + '] value=' + txseq + '><br></div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('input').style.display = 'none';
	}

	function addrights(s) {
		var rights = document.getElementById('rights').value;
		outrights.push(rights);

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>权益项[' + total.length + ']：' + rights + '<br></div>';
		blockstack.push({'block':'block' + seq, 'type':2});
		seq++;
		document.getElementById('output').style.display = 'none';
	}

/*
	function addoutput(s) {
		var receiver = document.getElementById('receiver').value;
		var amount = document.getElementById('amount').value;
		var rights = document.getElementById('rights').value;

		if (rights != '') {
			outrights.push(rights);
		}

		total.push({'type':'output', 'receiver':receiver, 'amount':amount, 'rights':outrights, 'seq':seq});
		outrights = [];

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>输出[' + total.length + ']：<input type=text id=receiver[' + seq + '] value=' + receiver + '> , <input type=text id=amount[' + seq + '] value=' + amount + '><br></div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('output').style.display = 'none';
	}
*/
	function addoutput(s) {
		var tokentype = parseInt(document.getElementById('tokentype').value);
		var receiver = document.getElementById('receiver').value;
		var amount = document.getElementById('amount').value;
		var rights = document.getElementById('rights').value;

		if (rights != '') {
			outrights.push(rights);
		}

		total.push({'type':'output', 'receiver':receiver, 'amount':amount, 'rights':outrights, 'tokentype':tokentype, 'seq':seq});
		outrights = [];

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>输出[' + total.length + ']：' + receiver + ' , ' + tokentype + ' : ' + amount + '</div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('output').style.display = 'none';
	}

	function addcontractcall() {
		var script = document.getElementById('contractcall').value;
		var amount = document.getElementById('callamount').value;

		total.push({'type':'output', 'receiver':'contract', 'script':script, 'amount':amount, 'seq':seq});
		outrights = [];

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>输出[' + total.length + ']：' + script + '<br></div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('contract').style.display = 'none';
	}

	function addoutput2(s) {
		var receiver = document.getElementById('receiver2').value;
		var addition = document.getElementById('addition').value;
		var initem = document.getElementById('initem').value;

		if (rights != '') {
			outrights.push(rights);
		}

		total.push({'type':'divide', 'receiver':receiver, 'addition':addition, 'initem':initem, 'seq':seq});
		outrights = [];

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>拆分[' + total.length + ']：<input type=text id=receiver2[' + seq + '] value=' + receiver + '> , <input type=text id=initem[' + seq + '] value=' + initem + '>， <input type=text id=addition[' + seq + '] value=' + addition + '><br></div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('output2').style.display = 'none';
	}

	function addoutput3(s) {
		var receiver = document.getElementById('receiver3').value;
		var initem1 = document.getElementById('initem1').value;
		var initem2 = document.getElementById('initem2').value;

		if (rights != '') {
			outrights.push(rights);
		}

		total.push({'type':'merge', 'receiver':receiver, 'initem':[initem1, initem2], 'seq':seq});
		outrights = [];

		document.getElementById('params').innerHTML += '<div id=block' + seq + '>拆分[' + total.length + ']：<input type=text id=receiver3['+ seq + '] value=' + receiver + '> , <input type=text id=initem1[' + seq + '] value=' + initem1 + '>， <input type=text id=initem2[' + seq + '] value=' + initem2 + '><br></div>';
		blockstack.push({'block':'block' + seq, 'type':0});
		seq++;
		document.getElementById('output3').style.display = 'none';
	}

	function resubmitrawtx() {
		var d = JSON.stringify(total);

		var url = './index.php?module=rpc&MOD_op=createrawtransaction&submiting=1';
		$.ajax({
			url: url,
			type:'post',
			data:{ 'module': "rpc", 'MOD_op' : "createrawtransaction", 'submitting' : 1, 'data': d },
			success: function(resp) {
				var app = document.getElementById('application');
				var appval = app.options[app.selectedIndex].value;
				var loca = document.getElementById('location');
				var loc = loca.options[loca.selectedIndex].value;
//				document.getElementById('result').innerHTML += url + '&data=' + d + '<hr>' + resp;
				document.body.innerHTML = resp + '<hr>Please sign the transaction and submit the signed raw transaction using the form below:<br>' + document.getElementById('signingform').innerHTML;
				document.getElementById('seldapp').value = appval;
				document.getElementById('seldloc').value = loc;
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
	}

	function submitrawlandtx() {
//		document.getElementById('editland').style.display = 'none';

		var d = JSON.stringify(total);

		var url = './index.php?module=rpc&MOD_op=createrawtransaction&submiting=1';
		$.ajax({
			url: url,
			type:'post',
			data:{ 'module': "rpc", 'MOD_op' : "createrawtransaction", 'submitting' : 1, 'data': d },
			success: function(resp) {
				var spt = resp.split("[rawtx] => ")[1].split(/\s/)[0];

				document.body.innerHTML = '<div style="margin:20px;">Land price: ' + landvalue + ' BOEs<br>Raw Transaction:<br><textarea rows=10 cols=80>' + spt + "</textarea><hr>Please paste the above raw transaction to 'Land Definition' box.</div>";
/*
				var loca = document.getElementById('location');
				var loc = loca.options[loca.selectedIndex].value;
//				document.getElementById('result').innerHTML += url + '&data=' + d + '<hr>' + resp;
				document.body.innerHTML = "Unsigned raw transaction:<br>" + resp + '<hr>Please sign the above raw transaction using signing mode 2-to-2 (see <a href=./index.php?module=rpc&MOD_op=signrawtransaction&outputs=DOUBLE target=_blank>example</a>), and submit the signed raw transaction using the form below:<br>' + document.getElementById('signingform').innerHTML;
				document.getElementById('seldapp').value = appval;
				document.getElementById('seldloc').value = loc;
*/
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
	}

	function submitrawtx() {
		var d = JSON.stringify(total);

		$.ajax({
			url:'./index.php?module=rpc&MOD_op=createrawtransaction&submiting=1',
			type:'post',
			data:{ 'module': "rpc", 'MOD_op' : "createrawtransaction", 'submitting' : 1, 'data': d },
			success: function(resp) {
				document.body.innerHTML += resp;
			},
			error: function(a, v, c) {
				alert("系统错误。");
			}
		});
	}

	function updatevertex() {
		link.setMap(null);

		for (var i = 0; i < 21; i++) {
			ps[i].lat = parseFloat(document.getElementById('lat[' + i + ']').value);
			ps[i].lng = parseFloat(document.getElementById('lng[' + i + ']').value);
		}

		link = new google.maps.Polygon({
			paths: [ps],
			strokeColor: '#FF0000',
			strokeOpacity: 0.8,
			strokeWeight: 2,
			fillColor: '#00FF00',
			fillOpacity: 0.35
		});

		link.setMap(map);
	}


  function loadScript(url) {
	var script = document.createElement("script");
	var head = document.head || document.getElementsByTagName("head")[0] || document.documentElement;
	script.async = true;
	script.type = "text/javascript";
	script.src = url;
	head.insertBefore(script, head.firstChild);
  }

  function getFormJson(frm) {
	var o = {};
	var a = $(frm).serializeArray();
	$.each(a, function () {
		if (o[this.name] !== undefined) {
			if (!o[this.name].push) {
				o[this.name] = [o[this.name]];
			}
			o[this.name].push(this.value || '');
		} else {
			o[this.name] = this.value || '';
		}
	});
			
	return o;
  }

