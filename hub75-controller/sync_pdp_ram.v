    // #( 
    //     parameter ADDR_WIDTH = 10,
    //     parameter DATA_WIDTH = 8,
    //     parameter DEPTH = 1024,
    // )

module sync_pdp_ram
    (
        input                   write_clk,
        input [10-1:0]          write_addr,
        input [8-1:0]           write_data,
        input                   write_en,
        input                   read_clk,
        input [10-1:0]          read_addr,
        output [8-1:0]          read_data,
        input                   read_en
    );

    reg [8-1:0] mem [1024];
    reg [8-1:0] tmp_data;

    always @ (posedge write_clk) begin
        if (write_en)
            mem[write_addr] <= write_data;
    end

    always @ (posedge read_clk) begin
       if (read_en) 
            tmp_data <= mem[read_addr];
    end

    assign read_data = read_en ? tmp_data : 'hz;
endmodule
