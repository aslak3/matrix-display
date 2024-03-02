module sync_pdp_ram #(parameter BITS_PER_PIXEL=0)
    (
        input                       buffer_toggle,
        input                       write_clk,
        input [10:0]                write_addr,
        input [BITS_PER_PIXEL-1:0]  write_data,
        input                       write_en,
        input                       read_clk,
        input [9:0]                 read_addr,
        output [BITS_PER_PIXEL-1:0] read_data_top,
        output [BITS_PER_PIXEL-1:0] read_data_bottom,
        input                        read_en
    );

    reg [BITS_PER_PIXEL-1:0] mem_top [64*16*2];
    reg [BITS_PER_PIXEL-1:0] mem_bottom [64*16*2];
    reg [BITS_PER_PIXEL-1:0] tmp_data_top;
    reg [BITS_PER_PIXEL-1:0] tmp_data_bottom;

    always @ (posedge write_clk) begin
        if (write_en) begin
            if (write_addr[10] == 1'b0)
                mem_top[{buffer_toggle, write_addr[9:0]}] <= write_data;
            else
                mem_bottom[{buffer_toggle, write_addr[9:0]}] <= write_data;
        end
    end

    always @ (posedge read_clk) begin
       if (read_en) begin
            tmp_data_top <= mem_top[{!buffer_toggle, read_addr}];
            tmp_data_bottom <= mem_bottom[{!buffer_toggle, read_addr}];
       end
    end

    assign read_data_top = read_en ? tmp_data_top : {BITS_PER_PIXEL{1'bz}};
    assign read_data_bottom = read_en ? tmp_data_bottom : {BITS_PER_PIXEL{1'bz}};
endmodule
