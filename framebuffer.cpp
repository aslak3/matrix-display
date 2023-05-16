#include <stdio.h>

#include "pico/stdlib.h"

#include "framebuffer.hpp"

framebuffer::framebuffer() {
}

void framebuffer::clear(void) {
    for (int r = 0; r < FB_HEIGHT; r++) {
        for (int c = 0; c < FB_WIDTH; c++) {
            background_rgb[r][c] = (rgb_t) {};
        }
    }
}

void framebuffer::point(int x, int y, rgb_t rgb)
{
    background_rgb[y][x] = rgb;
}

void framebuffer::hollowbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int p = x; p < x + width; p++) {
        background_rgb[y][p] = rgb;
        background_rgb[y + height - 1][p] = rgb;
    }

    for (int p = y; p < y + height; p++) {
        background_rgb[p][x] = rgb;
        background_rgb[p][x + width - 1] = rgb;
    }
}

void framebuffer::filledbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            background_rgb[r][c] = rgb;
        }
    }
}

int framebuffer::printchar(font_t *font, int x, int y, char c, rgb_t rgb, bool length_only)
{
    int index = (int)(c - ' ');
    int count = 0;
    int width = 0;
    int width_allocation = 0;

    if (font->lv_font_glyph_dsc) {
        count = font->lv_font_glyph_dsc[index].glyph_index;
        width = font->lv_font_glyph_dsc[index].w_px;
        width_allocation = width + 1;
    }
    else {
        if (font->fixed_width != 0) {
            count = index * font->height;
            width = font->fixed_width;
            width_allocation = width;
        }
        else {
            count = index * (font->height + 1);
            width = font->data[count];
            count++;
            width_allocation = width + 1;
        }
    }

    if (length_only) {
        return width_allocation;
    }

    for (int r = 0; r < font->height; r++) {
        if (font->lv_font_glyph_dsc) {
            for (int c = 0; c < width; c++) {
                if (font->data[count] > 0x20 && x + c < FB_WIDTH && x + c >= 0) {
                    background_rgb[y - r][x + c] = rgb_t {
                        .red =   (uint8_t)((uint32_t)(font->data[count] * rgb.red) / 256),
                        .green = (uint8_t)((uint32_t)(font->data[count] * rgb.green) / 256),
                        .blue =  (uint8_t)((uint32_t)(font->data[count] * rgb.blue) / 256),
                    };
                }
                count++;
            }
        }
        else {
            uint8_t byte = font->data[count];
            for (int c = 0; c < width; c++) {
                if (x + c < FB_WIDTH && x + c >= 0) {
                    if (byte & 0x80) {
                        background_rgb[font->height + (y - r)][x + c] = rgb;
                    }
                }
                byte <<= 1;
            }
            count++;
        }
    }
    return width_allocation;
}

void framebuffer::printstring(font_t *font, int x, int y, const char *s, rgb_t rgb)
{
    int running_x = x;
    for (; *s; s++) {
        running_x += printchar(font, running_x, y, *s, rgb, false);
    }
}

int framebuffer::stringlength(font_t *font, const char *s)
{
    int running_x = 0;
    for (; *s; s++) {
        running_x += printchar(font, running_x, 0, *s, (rgb_t) {}, true);
    }
    return running_x;
}


void framebuffer::showimage(image_t *image, int x, int y)
{
    const image_dsc_t *image_dsc = image->image_dsc;
    uint8_t *off = (uint8_t *) image_dsc->data;

    for (int c = 0; c < image_dsc->width; c++) {
        for (int r = 0; r < image_dsc->height; r++) {
            background_rgb[r + y][c + x] = rgb_t {
                .red = *(off + 0),
                .green = *(off + 1),
                .blue = *(off + 2),
            };

            off += 4;
        }
    }
}
