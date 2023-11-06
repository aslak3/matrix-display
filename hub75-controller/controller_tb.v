module controller_tb;
    reg n_reset;
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

    reg [15:0] input_image [64 * 32];

    localparam period = 1;

    controller dut (
        n_reset,
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

    reg [3:0] screen_red[64][32];
    reg [3:0] screen_green[64][32];
    reg [3:0] screen_blue[64][32];
    reg [7:0] hub75_oe_count;

    initial begin
        $dumpfile("controller.vcd");
        $dumpvars();

        n_reset = 1'b1;
        pixel_clk = 1'b0;
        spi_clk = 1'b0;
        spi_mosi = 1'b0;

        for (integer y_count = 0; y_count < 32; y_count++) begin
            for (integer x_count = 0; x_count < 64; x_count++) begin
                screen_red[x_count][y_count] = 4'h0;
                screen_green[x_count][y_count] = 4'h0;
                screen_blue[x_count][y_count] = 4'h0;
            end
        end

        hub75_oe_count = 8'b0;

        #period;
        n_reset = 1'b0;

        #period;
        n_reset = 1'b1;
    end

    integer write_buffers;
    integer write_pixel_count;
    integer write_bit_count;

    initial begin
        $readmemh("test-bars.txt", input_image);

        #(period * 10);

        for (write_buffers = 0; write_buffers < 2; write_buffers++) begin
            for (write_pixel_count = 0; write_pixel_count < 64 * 32; write_pixel_count++) begin
                for (write_bit_count = 0; write_bit_count < 16; write_bit_count++) begin
                    #period;
                    spi_clk = 1'b0;
                    spi_mosi = input_image[write_pixel_count][15 - write_bit_count];

                    #period;
                    spi_clk = 1'b1;
                end
            end
        end
    end

    initial begin
        #(period * (10 + (64 * 32 * 16 * 2 * 2)));

        forever begin
            pixel_clk = 1'b1;
            #period;

            pixel_clk = 1'b0;
            #period;
        end
    end

    reg [5:0] this_line_x = 6'b000000;
    reg [1:0] this_line_red[64];
    reg [1:0] this_line_green[64];
    reg [1:0] this_line_blue[64];

    reg [1:0] latched_line_red[64];
    reg [1:0] latched_line_green[64];
    reg [1:0] latched_line_blue[64];

    always @ (posedge hub75_clk) begin
        this_line_red[this_line_x] <= hub75_red;
        this_line_green[this_line_x] <= hub75_green;
        this_line_blue[this_line_x] <= hub75_blue;
        this_line_x <= this_line_x + 6'b1;
    end

    always @ (posedge pixel_clk) begin
        if (hub75_latch == 1'b1) begin
            for (integer x_count = 0; x_count < 64; x_count++) begin
                latched_line_red[x_count] <= this_line_red[x_count];
                latched_line_green[x_count] <= this_line_green[x_count];
                latched_line_blue[x_count] <= this_line_blue[x_count];
            end
        end
    end

    always @ (posedge pixel_clk) begin
        if (hub75_oe == 1'b0) begin
            for (integer x_count = 0; x_count < 64; x_count++) begin
                screen_red[x_count][{1'b0, hub75_addr}] <= screen_red[x_count][{1'b0, hub75_addr}] +
                    {3'b000, latched_line_red[x_count][0]};
                screen_red[x_count][{1'b1, hub75_addr}] <= screen_red[x_count][{1'b1, hub75_addr}] +
                    {3'b000, latched_line_red[x_count][1]};
                screen_green[x_count][{1'b0, hub75_addr}] <= screen_green[x_count][{1'b0, hub75_addr}] +
                    {3'b000, latched_line_green[x_count][0]};
                screen_green[x_count][{1'b1, hub75_addr}] <= screen_green[x_count][{1'b1, hub75_addr}] +
                    {3'b000, latched_line_green[x_count][1]};
                screen_blue[x_count][{1'b0, hub75_addr}] <= screen_blue[x_count][{1'b0, hub75_addr}] +
                    {3'b000, latched_line_blue[x_count][0]};
                screen_blue[x_count][{1'b1, hub75_addr}] <= screen_blue[x_count][{1'b1, hub75_addr}] +
                    {3'b000, latched_line_blue[x_count][1]};

                // $write("%01x", screen_blue[x_count][{1'b1, hub75_addr}]);
            end

            if (hub75_oe_count == 8'b11111111) begin
                for (integer y_count = 0; y_count < 32; y_count++) begin
                    for (integer x_count = 0; x_count < 64; x_count++) begin
                        $display("%01x%01x%01x0", screen_red[x_count][y_count], screen_green[x_count][y_count],
                            screen_blue[x_count][y_count]);
                    end
                end
                $finish();
            end
            hub75_oe_count <= hub75_oe_count + 8'b1;
        end
    end

endmodule
