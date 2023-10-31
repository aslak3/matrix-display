module spi_slave_tb;
    reg reset;
    reg spi_clk;
    reg spi_mosi;
    wire [15:0] data;
    wire pixel_clock;

    localparam period = 1;

    spi_slave dup (
        reset, spi_clk, spi_mosi, data, pixel_clock
    );

    parameter [31:0] test_input = 32'h1234;

    integer bit_counter;

    initial begin
        $dumpfile("spi_slave.vcd");
        $dumpvars();

        reset = 1'b0;
        spi_clk = 1'b0;
        spi_mosi = 1'b0;

        #period;
        reset = 1'b1;

        #period;
        reset = 1'b0;

        #period
        for (bit_counter = 0; bit_counter < 16; bit_counter++) begin
            spi_clk = 1'b0;
            spi_mosi = test_input[15 - bit_counter];
            #period;

            spi_clk = 1'b1;
            #period;
        end

        $display("Got %04x", data);
        #(period * 10);
    end
endmodule
