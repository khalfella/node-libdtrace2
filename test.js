var libdtrace = require('./build/Release/dtrace');


try {
	var dtc = new libdtrace.DTraceConsumer();
} catch (e) {
	console.log('Exception has been thrown');
}
