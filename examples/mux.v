// Output should always be the first signal in port list
primitive mux (out, sel, a, b);
	output 	out;
	input 	sel, a, b;

	table
		// sel 	a 	b 		out
			0 	1 	? 	: 	1;
			0 	0 	? 	: 	0;
			1 	? 	0 	: 	0;
			1 	? 	1 	: 	1;
			x 	0 	0 	: 	0;
			x 	1 	1 	: 	1;
	endtable
endprimitive