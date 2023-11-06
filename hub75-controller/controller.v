module controller
    (
        input n_reset,
        input pixel_clk,
        output reg [1:0] hub75_red,
        output reg [1:0] hub75_green,
        output reg [1:0] hub75_blue,
        output reg [3:0] hub75_addr,
        output hub75_clk,
        output reg hub75_latch,
        output reg hub75_oe,
        input spi_clk,
        input spi_mosi
    );

    wire reset = ~n_reset;

    wire [15:0] write_data;
    wire write_pixel_clk;
    reg [11:0] write_addr; // top bit is the double-buffer flipper
    reg [9 + 4:0] read_addr; // top four bits are the intensity we are checking for
    wire [15:0] read_data_top;
    wire [15:0] read_data_bottom;
    integer read_state;

    spi_slave spi_slave (
        reset, spi_clk, spi_mosi, write_data, write_pixel_clk
    );

    always @ (posedge reset or negedge write_pixel_clk) begin
        if (reset == 1'b1) begin
            // Won't get the first word until the first clock, not before
            write_addr <= 12'b111111111111;
        end else begin
            write_addr <= write_addr + 12'b1;
        end
    end

    sync_pdp_ram sync_pdp_ram (
        write_addr[11],
        write_pixel_clk, write_addr[10:0], write_data, 1'b1,
        pixel_clk, read_addr[9:0], read_data_top, read_data_bottom, 1'b1
    );

    localparam READ_STATE_RESET = 0,
        READ_STATE_PIXELS = 1,
        READ_STATE_SET_LATCH = 2,
        READ_STATE_END_OF_LINE = 3,
        READ_STATE_NEXT_LINE = 4;

    wire [3:0] intensity_test = read_addr [13:10];

    always @ (posedge reset or negedge pixel_clk) begin
        if (reset == 1'b1) begin
            read_state <= READ_STATE_RESET;
            read_addr <= 14'b0;
        end else begin
            case (read_state)
                READ_STATE_RESET: begin
                    hub75_red <= 2'b00;
                    hub75_green <= 2'b00;
                    hub75_blue <= 2'b00;
                    hub75_latch <= 1'b0;
                    hub75_oe <= 1'b1;
                    read_state <= READ_STATE_PIXELS;
                end

                READ_STATE_PIXELS: begin
                    hub75_red <= {
                        read_data_bottom[15:12] >= intensity_test ? 1'b1 : 1'b0,
                        read_data_top[15:12] >= intensity_test ? 1'b1 : 1'b0
                    };
                    hub75_green <= {
                        read_data_bottom[11:8] >= intensity_test ? 1'b1 : 1'b0,
                        read_data_top[11:8] >= intensity_test ? 1'b1 : 1'b0
                    };
                    hub75_blue <= {
                        read_data_bottom[7:4] >= intensity_test ? 1'b1 : 1'b0,
                        read_data_top[7:4] >= intensity_test ? 1'b1 : 1'b0
                    };

                    read_addr <= read_addr + 14'b1;
                    if (read_addr[5:0] == 6'b0) begin
                        read_state <= READ_STATE_SET_LATCH;
                    end
                end

                READ_STATE_SET_LATCH: begin
                    hub75_latch <= 1'b1;
                    read_state <= READ_STATE_END_OF_LINE;
                end

                READ_STATE_END_OF_LINE: begin
                    hub75_latch <= 1'b0;
                    hub75_oe <= 1'b0;
                    read_state <= READ_STATE_NEXT_LINE;
                end

                READ_STATE_NEXT_LINE: begin
                    hub75_oe <= 1'b1;
                    hub75_addr <= hub75_addr + 4'b1;
                    read_state <= READ_STATE_PIXELS;
                end
            endcase
        end
    end

    wire _unused_ok = &{1'b0,
        read_data_bottom[3:0],
        read_data_top[3:0],
        1'b0};

    //assign hub75_addr = read_addr[9:6];
    assign hub75_clk = pixel_clk ? read_state == READ_STATE_PIXELS : 1'b0;
endmodule
