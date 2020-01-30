var libdtrace = require('./build/Release/dtrace');


try {



	var dtc = new libdtrace.DTraceConsumer();
	var prog = 'BEGIN { trace("hello world"); }';
    
	dtc.strcompile(prog);

	dtc.go();

} catch (e) {
	console.log('Exception has been thrown', e.toString());
}
