
`timescale 1ns/1ns

module testbench();

typedef enum { RESET, LOAD, READY, RUN, POLL, STOP, ERROR } STATE;

parameter PERIOD = 10;

//import "DPI-C" function int main_call_from_sv(input int arg, input string argv);
import "DPI-C" context task main_call_from_sv(input int arg, input string argv);

// set current clock value
export "DPI-C" function dpi_clock;

// simulate delay cycles
export "DPI-C" task dpi_delay;

// synchrony
//export "DPI-C" task dpi_wait_finish;

//import "DPI-C" context task scheduler_once();

logic clk;

logic [63:0] clock;

always # (PERIOD/2) clk <= ~ clk;
always @(posedge clk) clock <= clock + 64'h01;

initial
begin
	$display("Entering in testbench");
	clk = 0;
	clock = 64'h00;
	#(PERIOD * 2);

	main_call_from_sv(8, "seq_main -d -b -i software/Image_data/color100.bin -n 3 -o");
	$display("Finished!");
	$finish;
end

function int dpi_clock(input int mode, input longint cycle);
	if(mode == 0) clock = cycle;
	else if(mode > 0) clock = clock + cycle; 

	$display("[dpi_clock] %08d", clock);
	return clock;
endfunction

task dpi_delay(input longint cycle);
	longint i;
	i = 0;
	$display("[dpi_delay] %08d", cycle);
	while(i != cycle) begin i = i + 1; #PERIOD; end
	$display("[dpi_delay] Leaving...");
endtask


// task dpi_wait_finish();
// 	$display("Enter dpi_wait_finish");
// 	while(1) begin $PERIOD; if(state == STOP || last_state == RUN) break; end
// 	$display("Leaving dpi_wait_finish");
// endtask

endmodule



