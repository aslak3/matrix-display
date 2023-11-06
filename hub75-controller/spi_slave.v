module spi_slave
    (
        input           reset,
        input           spi_clk,
        input           spi_mosi,
        output [15:0]   data,
        output          pixel_clk
    );

    reg [15:0] tmp_data = 16'b0;
    reg [15:0] last_data = 16'b0;
    reg [3:0] bit_counter = 4'b0;

    always @ (posedge reset or posedge spi_clk) begin
        if (reset == 1'b1) begin
            bit_counter <= 4'b0;
            last_data <= 16'b0;
        end else begin
            tmp_data[15 - bit_counter] <= spi_mosi;
            bit_counter <= bit_counter + 4'b1;

            if (bit_counter == 4'b0000) begin
                last_data <= tmp_data;
            end
        end
    end

    assign data = last_data;
    assign pixel_clk = ~bit_counter[3];
endmodule

