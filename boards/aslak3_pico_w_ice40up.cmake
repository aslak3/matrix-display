set(MATRIX_DISPLAY_SDK          PICO)
set(PICO_BOARD                  pico_w)

set(FRAME_DELAY_MS              15)

set(I2C_INTERFACE               1)
set(I2C_SDA_PIN                 20)
set(I2C_SCL_PIN                 21)

set(SPI_TO_FPGA                 1)

if(SPI_TO_FPGA)
set(FPGA_RESET_PIN              26)
else()
set(HUB75_RED1_PIN              2)
set(HUB75_ROWSEL_A_PIN          8)

set(HUB75_CLK_PIN               13)
set(HUB75_STROBE_PIN            14)
set(HUB75_OEN_PIN               15)
endif()
