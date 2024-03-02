module tb;
    parameter BITS_PER_PIXEL = 32;

    reg reset;
    reg spi_clk;
    reg spi_mosi;
    wire [BITS_PER_PIXEL-1:0] data;
    wire pixel_clk;

    localparam period = 1;

    spi_slave #(BITS_PER_PIXEL) dut ( reset, spi_clk, spi_mosi, data, pixel_clk);

    // TODO: Parameterise
    parameter [32+32+32-1:0] test_input = 96'hdeadbeefcabba6e000000000;

    integer word_counter;
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
            for (bit_counter = 0; bit_counter < 32+32+32; bit_counter++) begin
                spi_clk = 1'b0;
                spi_mosi = test_input[32+32+32 - 1 - bit_counter];
                #period;

                spi_clk = 1'b1;
                #period;
        end
    end

    always @ (posedge pixel_clk) begin
        $display("Got %x", data);
    end
endmodule
