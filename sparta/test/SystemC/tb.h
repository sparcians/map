#include <systemc.h>

SC_MODULE( tb )
{
	sc_in<bool> clk;
	sc_out<bool> rst;
	sc_out< sc_int<16> > inp;
	sc_out<bool> inp_vld;
        sc_in<bool> inp_rdy;

	sc_in< sc_int<16> > outp;
	sc_in<bool> outp_vld;
	sc_out<bool> outp_rdy;

	void source();
	void sink();

	FILE *outfp;
	sc_time start_time[64], end_time[64], clock_period;

	SC_CTOR( tb ){
	        SC_CTHREAD( source, clk.pos() );
		SC_CTHREAD( sink, clk.pos() );
	}

};
