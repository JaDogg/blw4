#include "32blit.hpp"
#include "gpu.hpp"
#include <iostream>

static GpuRenderer renderer = GpuRenderer::STRETCH_RENDER;
#ifndef PICO_BUILD
static uint32_t prev_row[TARGET_SIZE];
static uint32_t cur_row[TARGET_SIZE];
#endif
static int target_x = 0;
static int target_y = -1;


void set_render(GpuRenderer renderer_value) {
  renderer = renderer_value;
}
GpuRenderer get_render() {
  return renderer;
}


/**
 * Write rgb to given pointer (3 bytes are written)
 * @param target
 * @param colour
 * @return
 */
uint8_t *rgb(uint8_t *target, uint32_t colour) {
  uint8_t r = (((colour & 0xff0000) >> 8) >> 8);
  uint8_t g = ((colour & 0xff00) >> 8);
  uint8_t b = (colour & 0xff);
  *target++ = r;
  *target++ = g;
  *target++ = b;
  return target;
}

uint32_t blend(uint32_t colour1, uint32_t colour2) {
  uint8_t r1 = (((colour1 & 0xff0000) >> 8) >> 8);
  uint8_t g1 = ((colour1 & 0xff00) >> 8);
  uint8_t b1 = (colour1 & 0xff);
  uint8_t r2 = (((colour2 & 0xff0000) >> 8) >> 8);
  uint8_t g2 = ((colour2 & 0xff00) >> 8);
  uint8_t b2 = (colour2 & 0xff);
  auto r = (uint8_t)(0.5 * r1 + 0.5 * r2);
  auto g = (uint8_t)(0.5 * g1 + 0.5 * g2);
  auto b = (uint8_t)(0.5 * b1 + 0.5 * b2);
  return b | (g << 8) | ((r << 8) << 8);
}


void draw_point_target(int x, int y, uint32_t colour) {
  rgb(blit::screen.ptr(x_skip + x, y), colour);
}

void draw_point_centered(int x, int y, uint32_t colour) {
  rgb(blit::screen.ptr(x_center_skip + x, y_center_skip + y), colour);
}

void draw_point(int origin_x, int origin_y, uint32_t origin_colour) {
  int x, y;
  bool row_fill = false;
  if (origin_x == 0) {
#ifndef PICO_BUILD
    for (int pos = 0; pos < TARGET_SIZE; pos++) {
      prev_row[pos] = cur_row[pos];
    }
#endif
    target_x = 0;
    target_y++;
    if ((origin_y + 1) % 2 == 0) {
      target_y++;
      row_fill = true;
    }
  } else {
    target_x++;
  }
  if (target_y >= TARGET_SIZE) {
    target_y = 0;
  }
  uint32_t colour = origin_colour;
  y = target_y;
  if ((origin_x + 1) % 2 == 0) {
#ifndef PICO_BUILD
    uint32_t prev_colour = cur_row[target_x - 1];
    uint32_t blended = blend(prev_colour, colour);
    draw_point_target(target_x, y, blended);
    cur_row[target_x] = blended;
#endif
    target_x++;
  }
  x = target_x;
  draw_point_target(x, y, colour);
#ifndef PICO_BUILD
  cur_row[x] = colour;
  if (row_fill) {
    int row_to_fill = y - 1;
    for (int fill_x = 0; fill_x < TARGET_SIZE; fill_x++) {
      uint32_t blended_colour = blend(prev_row[fill_x], cur_row[fill_x]);
      draw_point_target(fill_x, row_to_fill, blended_colour);
    }
  }
#endif
}

extern "C" {

void wasm4_draw_1_5_x(const uint32_t *palette, const uint8_t *framebuffer) {
  int pixel = -1, c1, c2, c3, c4, x, y;
  const int framebuffer_items = WASM4_SIZE * WASM4_SIZE / WASM4_PIXELS_PER_BYTE;
  for (int n = 0; n < framebuffer_items; ++n) {
    uint8_t quartet = framebuffer[n];
    c1 = (quartet & 0b00000011) >> 0;
    c2 = (quartet & 0b00001100) >> 2;
    c3 = (quartet & 0b00110000) >> 4;
    c4 = (quartet & 0b11000000) >> 6;

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point(x, y, palette[c1]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point(x, y, palette[c2]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point(x, y, palette[c3]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point(x, y, palette[c4]);
  }
}

void wasm4_draw_center(const uint32_t *palette, const uint8_t *framebuffer) {
  int pixel = -1, c1, c2, c3, c4, x, y;
  const int framebuffer_items = WASM4_SIZE * WASM4_SIZE / WASM4_PIXELS_PER_BYTE;
  for (int n = 0; n < framebuffer_items; ++n) {
    uint8_t quartet = framebuffer[n];
    c1 = (quartet & 0b00000011) >> 0;
    c2 = (quartet & 0b00001100) >> 2;
    c3 = (quartet & 0b00110000) >> 4;
    c4 = (quartet & 0b11000000) >> 6;

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point_centered(x, y, palette[c1]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point_centered(x, y, palette[c2]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point_centered(x, y, palette[c3]);

    pixel++;
    y = pixel / WASM4_SIZE;
    x = pixel % WASM4_SIZE;
    draw_point_centered(x, y, palette[c4]);
  }
}

/**
 * callback for wasm4 drawing
 * @param palette
 * @param framebuffer
 */
void w4_windowComposite(const uint32_t *palette, const uint8_t *framebuffer) {
  if (renderer == GpuRenderer::STRETCH_RENDER) {
    wasm4_draw_1_5_x(palette, framebuffer);
  } else {
    wasm4_draw_center(palette, framebuffer);
  }
}
}
