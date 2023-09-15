module sync_pdp_ram_tb;
    reg buffer_toggle;
    reg write_clk;
    reg [10:0] write_addr;
    reg [31:0] write_data;
    reg write_en;

    reg read_clk;
    reg [9:0] read_addr;
    wire [31:0] read_data_top;
    wire [31:0] read_data_bottom;
    reg read_en;

    integer i;
    reg [31:0] input_image [64 * 32];

    localparam period = 1;

    sync_pdp_ram dup (
        buffer_toggle,
        write_clk, write_addr, write_data, write_en,
        read_clk, read_addr, read_data_top, read_data_bottom, read_en
    );

    initial begin
        buffer_toggle = 1'b0;
        write_clk = 1'b0;
        write_addr = 11'b0;
        write_data = 31'b0;
        write_en = 1'b0;
        read_clk = 1'b0;
        read_addr = 11'b0;
        read_en = 1'b0;

        $readmemh("test-bars.txt", input_image);

        write_en = 1;
        for (i = 0; i < 64 * 32; i++) begin
            #period;
            write_data = input_image[i];
            write_clk = 1'b1;

            #period
            write_clk = 1'b0;
            write_addr = write_addr + 1;
        end

        #period;
        buffer_toggle = 1'b1;
        write_en = 1'b0;
        read_en = 1'b1;

        for (i = 0; i < 64 * (32 / 2); i++) begin
            #period;
            read_clk = 1'b1;

            #period
            read_clk = 1'b0;
            $display("Read addr %d data_top: %x data_bottom: %x", read_addr, read_data_top, read_data_bottom);
            read_addr = read_addr + 1;
        end
    end    
endmodule
