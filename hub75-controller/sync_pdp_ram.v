module sync_pdp_ram
    (
        input                   buffer_toggle,
        input                   write_clk,
        input [10:0]            write_addr,
        input [15:0]            write_data,
        input                   write_en,
        input                   read_clk,
        input [9:0]             read_addr,
        output [15:0]           read_data_top,
        output [15:0]           read_data_bottom,
        input                   read_en
    );

    reg [15:0] mem_top [2048];
    reg [15:0] mem_bottom [2048];
    reg [15:0] tmp_data_top;
    reg [15:0] tmp_data_bottom;

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

    assign read_data_top = read_en ? tmp_data_top : 16'bzzzzzzzzzzzzzzzz;
    assign read_data_bottom = read_en ? tmp_data_bottom : 16'bzzzzzzzzzzzzzzzz;
endmodule
