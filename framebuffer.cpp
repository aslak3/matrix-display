#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include <FreeRTOS.h>
#include <task.h>

#include "framebuffer.h"

framebuffer::framebuffer()
{
    brightness = 128;
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

void framebuffer::hollow_box(int x, int y, int width, int height, rgb_t rgb)
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

void framebuffer::filled_box(int x, int y, int width, int height, rgb_t rgb)
{
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            set_pixel(c, r, rgb);
        }
    }
}

void framebuffer::shadow_box(int x, int y, int width, int height, uint8_t gamma)
{
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            rgb_t rgb = get_pixel(c, r);
            set_pixel(c, r, rgb, gamma);
        }
    }
}

int framebuffer::print_char(font_t *font, int x, int y, char c, rgb_t rgb, bool length_only)
{
    // Basic sanity please
    if (c < 0x20 || c > 0x7f) {
        // printf("Bad character in framebuffer::printchar(): %02x\n", (int) c);
        return 0;
    }

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
                if (font->data[count] > 0x10) {
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

void framebuffer::print_string(font_t *font, int x, int y, const char *s, rgb_t rgb)
{
    int running_x = x;
    for (; *s; s++) {
        running_x += print_char(font, running_x, y, *s, rgb, false);
    }
}

int framebuffer::string_length(font_t *font, const char *s)
{
    int running_x = 0;
    for (; *s; s++) {
        running_x += print_char(font, running_x, 0, *s, (rgb_t) {}, true);
    }
    return running_x;
}

void framebuffer::show_image(image_t *image, int x, int y)
{
    show_image(image, x, y, 255);
}

void framebuffer::show_image(image_t *image, int x, int y, uint8_t gamma)
{
    const image_dsc_t *image_dsc = image->image_dsc;
    uint8_t *off = (uint8_t *) image_dsc->data;

    for (int r = 0; r < image_dsc->height; r++) {
        for (int c = 0; c < image_dsc->width; c++) {
            rgb_t rgb = {
                red: *(off + 0),
                green: *(off + 1),
                blue: *(off + 2),
            };
            if (rgb.red || rgb.green || rgb.blue) {
                set_pixel(c + x, image_dsc->height + (y - r), rgb, gamma);
            }

            off += 4;
        }
    }
}

void framebuffer::set_brightness(uint8_t b)
{
    brightness = b;
}

void framebuffer::atomic_back_to_fore_copy(void)
{
    taskENTER_CRITICAL();
    memcpy(&foreground, &background, sizeof(fb_t));
    taskEXIT_CRITICAL();
}

void framebuffer::atomic_fore_copy_out(fb_t *out)
{
    taskENTER_CRITICAL();
    if (brightness == 255) {
        memcpy(out, foreground.rgb, sizeof(fb_t));
    }
    else {
        for (int c = 0; c < FB_WIDTH; c++) {
            for (int r = 0; r < FB_HEIGHT; r++) {
                rgb_t rgb = foreground.rgb[r][c];
                out->rgb[r][c] = {
                    red: (uint8_t)((uint32_t)(rgb.red * brightness) / 255),
                    green: (uint8_t)((uint32_t)(rgb.green * brightness) / 255),
                    blue: (uint8_t)((uint32_t)(rgb.blue * brightness) / 255),
                };
            }
        }
    }
    taskEXIT_CRITICAL();
}

////

void framebuffer::set_pixel(int x, int y, rgb_t rgb)
{
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        background.rgb[y][x] = rgb;
    }
}

rgb_t framebuffer::get_pixel(int x, int y)
{
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        return background.rgb[y][x];
    }
    else {
        return (rgb_t) {};
    }
}

void framebuffer::set_pixel(int x, int y, rgb_t rgb, uint8_t gamma)
{
    rgb_t adjusted_rgb = {
        red: (uint8_t)((uint32_t)(rgb.red * gamma) / 255),
        green: (uint8_t)((uint32_t)(rgb.green * gamma) / 255),
        blue: (uint8_t)((uint32_t)(rgb.blue * gamma) / 255),
    };
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        background.rgb[y][x] = adjusted_rgb;
    }
}