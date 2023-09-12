module spi_slave
    (
        input           reset,
        input           spi_clk,
        input           spi_mosi,
        output [31:0]   data,
        output          pixel_clock
    );

    reg [31:0] tmp_data;
    reg [31:0] last_data;
    reg [4:0] bit_counter;

    always @ (reset) begin
        if (reset == 1) begin
            bit_counter <= 5'b0;
            last_data <= 32'b0;
        end
    end

    always @ (posedge spi_clk) begin
        tmp_data[31 - bit_counter] = spi_mosi;
        bit_counter = bit_counter + 1;

        if (bit_counter == 5'b0) begin
            last_data = tmp_data;
        end
    end

    assign data = last_data;
    assign pixel_clock = !bit_counter[4];
endmodule

