var libdtrace = require('./build/Release/dtrace');


try {



	var dtc = new libdtrace.DTraceConsumer();
	var prog = 'BEGIN { trace("hello world"); }';
    
	dtc.strcompile(prog);

	dtc.go();

	dtc.consume(function(probe, rec) {
		console.log(rec);
	});

} catch (e) {
	console.log('Exception has been thrown', e.toString());
}
