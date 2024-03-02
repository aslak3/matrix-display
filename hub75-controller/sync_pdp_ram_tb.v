module tb;
    parameter BITS_PER_PIXEL = 32;

    reg buffer_toggle;
    reg write_clk;
    reg [10:0] write_addr;
    reg [BITS_PER_PIXEL-1:0] write_data;
    reg write_en;

    reg read_clk;
    reg [9:0] read_addr;
    wire [BITS_PER_PIXEL-1:0] read_data_top;
    wire [BITS_PER_PIXEL-1:0] read_data_bottom;
    reg read_en;

    integer i;
    reg [BITS_PER_PIXEL-1:0] input_image [64 * 32];

    localparam period = 1;

    sync_pdp_ram #(BITS_PER_PIXEL) dut (
        buffer_toggle,
        write_clk, write_addr, write_data, write_en,
        read_clk, read_addr, read_data_top, read_data_bottom, read_en
    );

    reg [20*8:0] test_bars_filename;

    initial begin
        buffer_toggle = 1'b0;
        write_clk = 1'b0;
        write_addr = 11'b0;
        write_data = {BITS_PER_PIXEL{1'b0}};
        write_en = 1'b0;
        read_clk = 1'b0;
        read_addr = 10'b0;
        read_en = 1'b0;

        $sformat(test_bars_filename, "test-bars-%0d.txt", BITS_PER_PIXEL);
        $readmemh(test_bars_filename, input_image);

        write_en = 1;
        for (i = 0; i < 64 * 32; i++) begin
            #period;
            write_data = input_image[i];
            write_clk = 1'b0;

            #period
            write_clk = 1'b1;
            write_addr = write_addr + 1;
        end

        #period;
        buffer_toggle = 1'b1;
        write_en = 1'b0;
        read_en = 1'b1;

        for (i = 0; i < 64 * (32 / 2); i++) begin
            read_clk = 1'b1;
            #period;

            read_clk = 1'b0;
            $display("Read addr %d data_top: %x data_bottom: %x", read_addr, read_data_top, read_data_bottom);
            read_addr = read_addr + 10'b1;
            #period;
        end
    end
endmodule
