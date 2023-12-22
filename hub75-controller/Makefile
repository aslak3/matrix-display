IVERILOG = iverilog -g 2012 -g io-range-error -Wall -Ptb.BITS_PER_PIXEL=$(BITS_PER_PIXEL)
VVP = vvp

YOSYS = yosys
PIN_DEF = constraints.pcf
DEVICE = up5k

NEXTPNR = nextpnr-ice40
ICEPACK = icepack
ICETIME = icetime
ICEPROG = iceprog

VERILATOR_LINT = verilator --lint-only --timing -GBITS_PER_PIXEL=$(BITS_PER_PIXEL)

# Configuration
BITS_PER_PIXEL = 23


all: sync_pdp_ram spi_slave controller

sync_pdp_ram: sync_pdp_ram.v sync_pdp_ram_tb.v
	$(VERILATOR_LINT) $^
	$(IVERILOG) -o $@ $^
spi_slave: spi_slave.v spi_slave_tb.v
	$(VERILATOR_LINT) $^
	$(IVERILOG) -o $@ $^
controller: controller.v controller_tb.v sync_pdp_ram.v spi_slave.v
	$(VERILATOR_LINT) $^
	$(IVERILOG) -o $@ $^

sync_pdp_ram-tests: sync_pdp_ram
	$(VVP) sync_pdp_ram
spi_slave-tests: spi_slave
	$(VVP) spi_slave
controller-tests: controller
	$(VVP) controller

tests: sync_pdp_ram-tests spi_slave-tests controller-tests


controller.json: controller.v sync_pdp_ram.v spi_slave.v
	$(YOSYS) -p 'chparam -set BITS_PER_PIXEL $(BITS_PER_PIXEL)' \
		-p 'synth_ice40 -top controller -json $@' $^

controller.asc: controller.json
	$(NEXTPNR) --up5k --package sg48 --pcf $(PIN_DEF) --json controller.json --asc $@

controller.bin: controller.asc
	$(ICEPACK) $^ $@

clean:
	rm -vf sync_pdp_ram spi_slave controller *.vcd *.json *.asc *.bin
