package main

import (
	"fmt"
	"os"
	"os/exec"

	"github.com/sergeymakinen/go-bmp"
)

func get_filename(s string) string {
	m := map[string]string{
		"0-0": "cloudy",
		"1-0": "hail",
		"2-0": "windy",
		"3-0": "sunny",
		"0-1": "clear_night",
		"1-1": "rainy",
		"2-1": "lightning",
		"3-1": "lightning_rainy",
		"0-2": "partlycloudy",
		"1-2": "pouring",
		"2-2": "snowy",
		"3-2": "fog",
		"0-3": "snowy_rainy",
		"1-3": "sunny_alt",
		"2-3": "fog_alt",
		"3-3": "rain_alt",
	}
	if m[s] != "" {
		return m[s]
	} else {
		return s
	}
}

func main() {
	args := os.Args[1:]

	if len(args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: <input impage> <size>\n")
		fmt.Fprintf(os.Stderr, "\tsize: AxB!, for an image A by B pixels\n")
		os.Exit(1)
	}

	input_filename := args[0]
	output_size := args[1]

	const input_width = 960
	const input_height = 1000
	const margin = 20
	const tiles_x = 4
	const tiles_y = 4

	const tile_width = (input_width / tiles_x) - (margin * 2)
	const tile_height = (input_height / tiles_y) - (margin * 2)

	err := os.Mkdir(output_size, 0755)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error running mkdir:", err, ", carrying on")
	}

	fmt.Printf("#include \"images.h\"\n")
	fmt.Printf("\n")

	var top_left_y = margin + 10

	for y_count := 0; y_count < tiles_x; y_count++ {
		var top_left_x = margin + 20
		for x_count := 0; x_count < tiles_y; x_count++ {
			crop := fmt.Sprintf("%dx%d+%d+%d", tile_width, tile_height, top_left_x, top_left_y)
			output_filename_x_y := get_filename(fmt.Sprintf("%d-%d", x_count, y_count))

			output_filename := fmt.Sprintf("%s/%s.bmp", output_size, output_filename_x_y)
			image_size_ignore_aspect := output_size + "!"
			cmd := exec.Command("convert", input_filename, "-depth", "32", "-crop", crop, "-scale", image_size_ignore_aspect, output_filename)
			err := cmd.Run()
			if err != nil {
				fmt.Fprintln(os.Stderr, "Error: ", err)
			}

			f, _ := os.Open(output_filename)
			img, _ := bmp.Decode(f)

			bounds := img.Bounds()

			fmt.Printf("const uint8_t %s_%s[] = {\n", output_filename_x_y, output_size)
			for y := 0; y < bounds.Dy(); y++ {
				for x := 0; x < bounds.Dx(); x++ {
					pixel := img.At(x, y)
					r, g, b, _ := pixel.RGBA()
					fmt.Printf("\t0x%02x, 0x%02x, 0x%02x, 0x000,\n", r/256, g/256, b/256)
				}
				fmt.Printf("\t// end of line\n")
			}
			fmt.Printf("};\n")
			fmt.Printf("\n")

			fmt.Printf("const image_dsc_t %s_%s_dsc = {\n", output_filename_x_y, output_size)
			fmt.Printf("\twidth: %d,\n", bounds.Dx())
			fmt.Printf("\theight: %d,\n", bounds.Dx())
			fmt.Printf("\tdata: %s_%s,\n", output_filename_x_y, output_size)
			fmt.Printf("};\n")
			fmt.Printf("\n")

			top_left_x += tile_width + (margin * 2)
		}
		top_left_y += tile_width + (margin * 2)
	}
}
