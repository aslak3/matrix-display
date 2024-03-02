module tb #(BITS_PER_PIXEL=0);
    reg n_reset;
    reg clk;
    wire [1:0] hub75_red;
    wire [1:0] hub75_green;
    wire [1:0] hub75_blue;
    wire [3:0] hub75_addr;
    wire hub75_latch;
    wire hub75_clk;
    wire hub75_oe;
    reg spi_clk;
    reg spi_mosi;
    reg spi_miso;
    reg user_led;

    // parameter BITS_PER_PIXEL = 32;

    reg [BITS_PER_PIXEL-1:0] input_image [64 * 32];

    localparam period = 1;

    localparam BITS_PER_RGB = BITS_PER_PIXEL / 4;

    controller #(BITS_PER_PIXEL) dut (
        n_reset,
        clk,
        hub75_red,
        hub75_green,
        hub75_blue,
        hub75_addr,
        hub75_clk,
        hub75_latch,
        hub75_oe,
        spi_clk,
        spi_mosi,
        spi_miso,
        user_led,
    );

    initial begin
        $dumpfile("controller.vcd");
        $dumpvars();

        n_reset = 1'b1;
        clk = 1'b0;
        spi_clk = 1'b0;
        spi_mosi = 1'b0;

        #period;
        n_reset = 1'b0;

        #period;
        n_reset = 1'b1;
    end

    integer write_buffer_count;
    integer write_pixel_count;
    integer write_bit_count;

    reg [20*8:0] test_bars_filename;

    initial begin
        $sformat(test_bars_filename, "test-bars-%0d.txt", BITS_PER_PIXEL);
        $readmemh(test_bars_filename, input_image);

        #(period * 10);

        // Feed the image in twice; to get the buffers to swap we must start writing the second frame
        for (write_buffer_count = 0; write_buffer_count < 2; write_buffer_count++) begin
            for (write_pixel_count = 0; write_pixel_count < 64 * 32; write_pixel_count++) begin
                for (write_bit_count = 0; write_bit_count < BITS_PER_PIXEL; write_bit_count++) begin
                    #period;
                    spi_clk = 1'b0;
                    spi_mosi = input_image[write_pixel_count][BITS_PER_PIXEL - 1 - write_bit_count];

                    #period;
                    spi_clk = 1'b1;
                end
            end
        end

        // Drive the reader (display) clock
        forever begin
            clk = 1'b1;
            #period;

            clk = 1'b0;
            #period;
        end
    end

    reg [5:0] this_line_x = 6'b000000;
    reg [1:0] this_line_red[64];
    reg [1:0] this_line_green[64];
    reg [1:0] this_line_blue[64];

    // Pull in the individual rows into a local buffer (top and bottom at the same time)
    always @ (posedge hub75_clk) begin
        this_line_red[this_line_x] <= hub75_red;
        this_line_green[this_line_x] <= hub75_green;
        this_line_blue[this_line_x] <= hub75_blue;
        this_line_x <= this_line_x + 6'b1;
    end

    reg [1:0] latched_line_red[64];
    reg [1:0] latched_line_green[64];
    reg [1:0] latched_line_blue[64];

    // Copy the current line into the latched line on the latched clock edge
    always @ (posedge clk) begin
        if (hub75_latch == 1'b1) begin
            for (integer x_count = 0; x_count < 64; x_count++) begin
                latched_line_red[x_count] <= this_line_red[x_count];
                latched_line_green[x_count] <= this_line_green[x_count];
                latched_line_blue[x_count] <= this_line_blue[x_count];
            end
        end
    end

    reg [BITS_PER_RGB-1:0] screen_red[64][32];
    reg [BITS_PER_RGB-1:0] screen_green[64][32];
    reg [BITS_PER_RGB-1:0] screen_blue[64][32];
    reg [BITS_PER_RGB-1:0] screen_dummy = {BITS_PER_RGB{1'b0}};

    // Must init the screen memory as we add to it through the frame
    initial begin
        for (integer y_count = 0; y_count < 32; y_count++) begin
            for (integer x_count = 0; x_count < 64; x_count++) begin
                screen_red[x_count][y_count] = {BITS_PER_RGB{1'b0}};
                screen_green[x_count][y_count] = {BITS_PER_RGB{1'b0}};
                screen_blue[x_count][y_count] = {BITS_PER_RGB{1'b0}};
            end
        end
    end

    // Count is across the whole screen: 2^BITS_PER_PIXEL * 16
    reg [BITS_PER_RGB+4-1:0] hub75_oe_count = {BITS_PER_RGB+4{1'b0}};

    always @ (posedge clk) begin
        if (hub75_oe == 1'b0) begin
            // Sum up the intensities when the Output Enable input is asserted
            for (integer x_count = 0; x_count < 64; x_count++) begin
                screen_red[x_count][{1'b0, hub75_addr}] <= screen_red[x_count][{1'b0, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_red[x_count][0]};
                screen_red[x_count][{1'b1, hub75_addr}] <= screen_red[x_count][{1'b1, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_red[x_count][1]};
                screen_green[x_count][{1'b0, hub75_addr}] <= screen_green[x_count][{1'b0, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_green[x_count][0]};
                screen_green[x_count][{1'b1, hub75_addr}] <= screen_green[x_count][{1'b1, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_green[x_count][1]};
                screen_blue[x_count][{1'b0, hub75_addr}] <= screen_blue[x_count][{1'b0, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_blue[x_count][0]};
                screen_blue[x_count][{1'b1, hub75_addr}] <= screen_blue[x_count][{1'b1, hub75_addr}] +
                    {{BITS_PER_RGB-1{1'b0}}, latched_line_blue[x_count][1]};
            end

            // Now see if we have got enough lines for a screen, which is 16 lots of 32/2
            if (hub75_oe_count == {BITS_PER_RGB+4{1'b1}}) begin
                for (integer y_count = 0; y_count < 32; y_count++) begin
                    for (integer x_count = 0; x_count < 64; x_count++) begin
                        // Output the RGB0 of each pixel, which will turn back into a BMP
                        $display("%01x%01x%01x%01x", screen_red[x_count][y_count], screen_green[x_count][y_count],
                            screen_blue[x_count][y_count], screen_dummy);
                    end
                end
                $finish();
            end
            hub75_oe_count <= hub75_oe_count + {{BITS_PER_RGB+4-1{1'b0}}, 1'b1};
        end
    end
endmodule
