#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include <FreeRTOS.h>
#include <task.h>

#include "framebuffer.h"

framebuffer::framebuffer()
{
}

void framebuffer::clear(rgb_t rgb) {
    for (int r = 0; r < FB_HEIGHT; r++) {
        for (int c = 0; c < FB_WIDTH; c++) {
            set_pixel(c, r, rgb);
        }
    }
}

void framebuffer::point(int x, int y, rgb_t rgb)
{
    set_pixel(x, y, rgb);
}

void framebuffer::hollowbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int p = x; p < x + width; p++) {
        set_pixel(p, y, rgb);
        set_pixel(p, y + height - 1, rgb);
    }

    for (int p = y; p < y + height; p++) {
        set_pixel(x, p, rgb);
        set_pixel(x + width - 1, p, rgb);
    }
}

void framebuffer::filledbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            set_pixel(c, r, rgb);
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
                // Ignore nearly off pixels
                if (font->data[count] > 0x60) {
                    set_pixel(x + c, font->height + (y - r), (rgb_t) {
                        .red =   (uint8_t)((uint32_t)(font->data[count] * rgb.red) / 256),
                        .green = (uint8_t)((uint32_t)(font->data[count] * rgb.green) / 256),
                        .blue =  (uint8_t)((uint32_t)(font->data[count] * rgb.blue) / 256),
                    });
                }
                count++;
            }
        }
        else {
            uint8_t byte = font->data[count];
            for (int c = 0; c < width; c++) {
                if (x + c < FB_WIDTH && x + c >= 0) {
                    if (byte & 0x80) {
                        // Unlike 8bpp fonts, the off pixels are left alone
                        set_pixel(x + c, font->height + (y - r), rgb);
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
            set_pixel(c + x, r + y, (rgb_t) {
                .red = *(off + 0),
                .green = *(off + 1),
                .blue = *(off + 2),
            });

            off += 4;
        }
    }
}

void framebuffer::atomic_back_to_fore_copy(void)
{
    taskENTER_CRITICAL();
    memcpy(foreground_rgb, background_rgb, sizeof(rgb_t) * FB_HEIGHT * FB_WIDTH);
    taskEXIT_CRITICAL();
}

void framebuffer::atomic_fore_copy_out(rgb_t *out)
{
    taskENTER_CRITICAL();
    memcpy(out, foreground_rgb, sizeof(rgb_t) * FB_HEIGHT * FB_WIDTH);
    taskEXIT_CRITICAL();
}

////

void framebuffer::set_pixel(int x, int y, rgb_t rgb)
{
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        background_rgb[y][x] = rgb;
    }
}