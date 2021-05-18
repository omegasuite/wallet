require.config({
      paths: {
          omgutil: 'omgutil',
          crypto: 'crypto',
		  rand: 'rand',
      }
 });
 
var omgutil, Crypto;

require(['crypto', 'omgutil'], function (c, u) {
	omgutil = u;
	Crypto = c;
	reqready = true;
});
