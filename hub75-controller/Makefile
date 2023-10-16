IVERILOG = iverilog
VVP = vvp

OPTS = -g 2012 -g io-range-error -Wall

PIN_DEF = constraints.pcf
DEVICE = up5k

NEXTPNR = nextpnr-ice40
ICEPACK = icepack
ICETIME = icetime
ICEPROG = iceprog


all: sync_pdp_ram spi_slave controller

sync_pdp_ram: sync_pdp_ram.v sync_pdp_ram_tb.v
	$(IVERILOG) $(OPTS) -o $@ $^
spi_slave: spi_slave.v spi_slave_tb.v
	$(IVERILOG) $(OPTS) -o $@ $^

controller: controller.v controller_tb.v sync_pdp_ram.v spi_slave.v
	$(IVERILOG) $(OPTS) -o $@ $^

sync_pdp_ram-tests: sync_pdp_ram
	$(VVP) sync_pdp_ram
spi_slave-tests: spi_slave
	$(VVP) spi_slave

controller-tests: controller
	$(VVP) controller

tests: sync_pdp_ram-tests spi_slave-tests controller-tests

controller.json: controller.v sync_pdp_ram.v spi_slave.v
	yosys -p 'synth_ice40 -top controller -json $@' $^

controller.asc: controller.json
	$(NEXTPNR) --up5k --package sg48 --pcf $(PIN_DEF) --json controller.json --asc $@

controller.bin: controller.asc
	$(ICEPACK) $^ $@

clean:
	rm -vf sync_pdp_ram spi_slave controller *.vcd *.json *.asc *.bin
