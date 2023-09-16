module controller_tb;
    reg reset;
    reg pixel_clk;
    wire [1:0] hub75_red;
    wire [1:0] hub75_green;
    wire [1:0] hub75_blue;
    wire [3:0] hub75_addr;
    wire hub75_latch;
    wire hub75_clk;
    wire hub75_oe;
    reg spi_clk;
    reg spi_mosi;
    
    reg [31:0] input_image [64 * 32];

    localparam period = 1;

    controller dup (
        reset,
        pixel_clk,
        hub75_red,
        hub75_green,
        hub75_blue,
        hub75_addr,
        hub75_clk,
        hub75_latch,
        hub75_oe,
        spi_clk,
        spi_mosi
    );

    initial begin
        $dumpfile("controller.vcd");
        $dumpvars();

        reset = 1'b0;
        pixel_clk = 1'b0;
        spi_clk = 1'b0;
        spi_mosi = 1'b0;

        #period;
        reset = 1'b1;

        #period;
        reset = 1'b0;
    end

    integer write_buffers;
    integer write_pixel_count;
    integer write_bit_count;
    initial begin
        $readmemh("test-bars.txt", input_image);

        #(period * 10);

        for (write_buffers = 0; write_buffers < 2; write_buffers++) begin
            for (write_pixel_count = 0; write_pixel_count < 64 * 32; write_pixel_count++) begin
                for (write_bit_count = 0; write_bit_count < 32; write_bit_count++) begin
                    #period;
                    spi_clk = 1'b0;
                    spi_mosi = input_image[write_pixel_count][31 - write_bit_count];

                    #period;
                    spi_clk = 1'b1;
                end
            end
        end
    end

    integer read_pixel_count;
    initial begin
        #(period * 10 + (64 * 32 * 2));

        for (read_pixel_count = 0; read_pixel_count < 64 * 32 * 32 * 2; read_pixel_count++) begin
            pixel_clk = 1'b1;
            #period;

            pixel_clk = 1'b0;
            $display("Red %02b Green %02b Blue %02b", hub75_red, hub75_green, hub75_blue);
            #period;
        end
    end

endmodule
