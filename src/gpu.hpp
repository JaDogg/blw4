#pragma once

#define WASM4_SIZE 160
#define TARGET_SIZE 240
#if defined(PICO_BUILD)
#define TARGET_WIDTH 240
#else
#define TARGET_WIDTH 320
#endif
#define WASM4_PIXELS_PER_BYTE 4
static const int x_skip = (TARGET_WIDTH - TARGET_SIZE) / 2;
static const int x_center_skip = (TARGET_WIDTH - WASM4_SIZE) / 2;
static const int y_center_skip = (TARGET_SIZE - WASM4_SIZE) / 2;

enum class GpuRenderer {
  CENTER_RENDER,
  STRETCH_RENDER
};

void set_render(GpuRenderer renderer_value);
GpuRenderer get_render();
