module spi_slave #(parameter BITS_PER_PIXEL=0)
    (
        input               reset,
        input               spi_clk,
        input               spi_mosi,
        output reg [BITS_PER_PIXEL-1:0]   data,
        output reg          pixel_clk
    );

    localparam BITS_PER_PIXEL_CLOG2 = $clog2(BITS_PER_PIXEL);

    reg [BITS_PER_PIXEL-1:0] tmp_data = {BITS_PER_PIXEL{1'b0}};
    reg [BITS_PER_PIXEL-1:0] last_data = {BITS_PER_PIXEL{1'b0}};
    reg [BITS_PER_PIXEL_CLOG2-1:0] bit_counter = {BITS_PER_PIXEL_CLOG2{1'b0}};

    always @ (posedge reset or posedge spi_clk) begin
        if (reset == 1'b1) begin
            bit_counter <= {BITS_PER_PIXEL_CLOG2{1'b1}};
            data <= {BITS_PER_PIXEL{1'b0}};
            pixel_clk <= 1'b0;
        end else begin
            tmp_data[bit_counter] <= spi_mosi;
            bit_counter <= bit_counter - {{BITS_PER_PIXEL_CLOG2 - 1{1'b0}}, 1'b1};

            // The first read will be rubbish
            if (bit_counter == {BITS_PER_PIXEL_CLOG2{1'b1}}) begin
                data <= tmp_data;
            end
            pixel_clk <= bit_counter[BITS_PER_PIXEL_CLOG2 - 1];
        end
    end
endmodule

