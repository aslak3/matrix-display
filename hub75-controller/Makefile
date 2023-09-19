IVERILOG = iverilog
VVP = vvp

OPTS = -g 2012 -g io-range-error -Wall

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

clean:
	rm -vf sync_pdp_ram spi_slave controller *.vcd
