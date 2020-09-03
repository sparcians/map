#include "tb.h"


void tb::source(){
	//Reset pulse generation
	inp.write( 0 );
	inp_vld.write( 0 );
	rst.write( 1 );
	wait();
	rst.write( 0 );
	wait();

	sc_int<16> tmp;

	//Send stimulus to DUT (Design under test)
	for( int i = 0; i < 64; i++){
		if (i > 23 && i < 29)
			tmp = 256;
		else
			tmp = 0;
		inp_vld.write( 1 );
		inp.write( tmp );
		start_time[i] = sc_time_stamp();
		do{
			wait();
		} while( !inp_rdy.read() );
		inp_vld.write( 0 );
	}

	// Hanging simulation guard condition
	wait(10000);
	printf("Hanging simulation stopped by TB source thread. Please check DUT module.\n");
	sc_stop();
}

void tb::sink(){
	sc_int<16> indata;

	//Extract clock period
	sc_clock *clk_p = dynamic_cast<sc_clock*>(clk.get_interface());
	clock_period = clk_p->period();

	//Create and open simulation output results file
	char output_file[256];
	sprintf( output_file, "./output.dat" );
	outfp = fopen( output_file, "w" );
	if ( outfp == NULL ){
		printf("Couldn't open output.dat file for writing.\n");
		exit(0);
	}

	//Initialize port
	outp_rdy.write( 0 );

	double total_cycles = 0;

	//Read output coming from DUT (Design under test)
	for( int i = 0; i < 64; i++){

		outp_rdy.write( 1 );
		do{
			wait();
		} while ( !outp_vld.read() );
		indata = outp.read();
		end_time[i] = sc_time_stamp();
		total_cycles += (end_time[i] - start_time[i]) / clock_period;
		outp_rdy.write( 0 );

		fprintf( outfp, "%d\n", (int)indata );
		cout << i << " :\t" <<indata.to_int() << endl;
	}
	double total_throughput = (start_time[63] - start_time[0]) / clock_period;

	//Print latency
	printf( "Average latency is %g cycles.\n", (double)(total_cycles/64) );
	printf( "Average throughput is %g cycles per input.\n", (double)(total_throughput/63) );

	//Close file
	fclose(outfp);
	//End simulation
	sc_stop();
}
