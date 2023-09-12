module sync_pdp_ram_tb;
    reg write_clk;
    reg [10-1:0] write_addr;
    reg [8-1:0] write_data;
    reg write_en;

    reg read_clk;
    reg [10-1:0] read_addr;
    wire [8-1:0] read_data;
    reg read_en;

    localparam period = 1;

    sync_pdp_ram dup (
        write_clk, write_addr, write_data, write_en,
        read_clk, read_addr, read_data, read_en
    );

    initial begin
        write_clk = 1'b0;
        write_addr = 10'b0;
        write_data = 8'b0;
        write_en = 1'b1;
        read_clk = 1'b0;
        read_addr = 10'b0;
        read_en = 1'b0;

        #period
        write_clk = 1'b1;
        write_addr = 1'b1;
        write_data = 8'h2a;
        write_en = 1'b1;
        
        #period
        write_clk = 1'b0;

        #period
        write_en = 1'b0;
        read_en = 1'b1;

        for (read_addr = 0; read_addr < 10; read_addr++) begin
            #period
            read_clk = 1'b1;

            #period
            read_clk = 1'b0;
            $display("Read addr %d data: %x", read_addr, read_data);
        end
    end    
endmodule
