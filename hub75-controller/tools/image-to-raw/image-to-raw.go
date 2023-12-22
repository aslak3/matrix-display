package main

import (
	"fmt"
	"os"

	"github.com/sergeymakinen/go-bmp"
)

func output_bmp_as_raw(filename string) {
	f, _ := os.Open(filename)
	img, _ := bmp.Decode(f)

	bounds := img.Bounds()

	for y := 0; y < bounds.Dy(); y++ {
		for x := 0; x < bounds.Dx(); x++ {
			pixel := img.At(x, y)
			r, g, b, _ := pixel.RGBA()
			fmt.Printf("%02x%02x%02x00\n", r/256, g/256, b/256)
		}
	}
}

func main() {
	args := os.Args[1:]

	if len(args) < 1 {
		fmt.Fprintf(os.Stderr, "Filename not specified\n")
		os.Exit(1)
	}
	input_filename := args[0]

	output_bmp_as_raw(input_filename)
}
