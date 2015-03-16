
DFLAGS = -DDPI
CFLAGS = $(DFLAGS)

all: header
	rm -rf work
	vlib work
	vlog testbench/testbench.sv
	vlog -ccflags "$(CFLAGS)" software/*.c

vsim:
	vsim -c  testbench

header:
	rm -rf work
	vlib work
	vlog -dpiheader software/dpiheader.h testbench/testbench.sv
