require.config({
      paths: {
          omgutil: 'omgutil',
          crypto: 'crypto',
          omega: 'omega',
      }
 });
 
var omegaDB, omgutil, omg, crypto;
var version = "0.0";

 require(['omega', 'omgutil', 'crypto'], function (omega, u, c) {
	omg = omega;
	omgutil = u;
	crypto = c;

	omegaDB.transaction(function (tx) {
		tx.executeSql("select * from setting_table where item='version'", [], function (tx, res) {
			if (res.rows.length > 0) version = res.rows[0].content;
		}, function (err) {});
	});
});
