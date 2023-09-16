module controller
    (
        input reset,
        input pixel_clk,
        output [1:0] hub75_red,
        output [1:0] hub75_green,
        output [1:0] hub75_blue,
        output [3:0] hub75_addr,
        output hub75_clk,
        output hub75_latch,
        output hub75_oe,
        input spi_clk,
        input spi_mosi
    );

    wire [31:0] write_data;
    wire write_pixel_clk;
    reg [11:0] write_addr; // top bit is the double-buffer flipper
    reg [9:0] read_addr;
    wire [31:0] read_data_top;
    wire [31:0] read_data_bottom;

    always @ (reset) begin
        if (reset == 1'b1) begin
            write_addr <= 11'b0;
            read_addr <= 10'b0;
        end
    end

    spi_slave spi_slave (
        reset, spi_clk, spi_mosi, write_data, write_pixel_clk
    );

    always @ (negedge write_pixel_clk) begin
        write_addr = write_addr + 1;
    end

    always @ (negedge pixel_clk) begin
        read_addr = read_addr + 1;
    end

    sync_pdp_ram sync_pdp_ram (
        write_addr[11],
        write_pixel_clk, write_addr[10:0], write_data, 1'b1,
        pixel_clk, read_addr, read_data_top, read_data_bottom, 1'b1
    );

    assign hub75_red = {read_data_top[31], read_data_bottom[31]};
    assign hub75_green = {read_data_top[23], read_data_bottom[23]};
    assign hub75_blue = {read_data_top[15], read_data_bottom[15]};

endmodule
