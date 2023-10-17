module spi_slave
    (
        input           reset,
        input           spi_clk,
        input           spi_mosi,
        output [15:0]   data,
        output          pixel_clock
    );

    reg [15:0] tmp_data;
    reg [15:0] last_data;
    reg [3:0] bit_counter;

    always @ (posedge reset or posedge spi_clk) begin
        if (reset == 1) begin
            bit_counter <= 5'b0;
            last_data <= 32'b0;
        end else begin
            tmp_data[15 - bit_counter] = spi_mosi;
            bit_counter = bit_counter + 1;

            if (bit_counter == 4'b0) begin
                last_data = tmp_data;
            end
        end
    end

    assign data = last_data;
    assign pixel_clock = ~bit_counter[3];
endmodule

