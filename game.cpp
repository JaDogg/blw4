#include "32blit.hpp"
#include "src/apu.hpp"
#include "src/gpu.hpp"
#include <cstring>
#include <iostream>

extern "C" {
#include "src/runtime.h"
#include "src/wasm.h"
}



enum class EmulatorState { CART_LOADING, CART_LOADED, CART_SELECTION };

static w4_Disk disk_storage_data{0, {}};
static bool first_time = true;
static uint32_t prev_time_ms = 0;
static int mouse_x = 0;
static int mouse_y = 0;
static EmulatorState emulator_state = EmulatorState::CART_SELECTION;
static blit::File cart_file{};
static std::vector<std::string> cart_files;
static int cart_file_idx = 0;
static int cart_file_render_start = 0;
static const int cart_file_render_max = 10;
static const std::string wasm_extension = ".wasm";

void load_cart(const std::string &cart_file_path);
void initialize_wasm4();
void render_selector();
void update_selector();
void clamp_cart_idx();
// ref:
// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool endswith(std::string const &input_string, std::string const &ending) {
  if (input_string.length() >= ending.length()) {
    return (0 == input_string.compare(input_string.length() - ending.length(),
                                      ending.length(), ending));
  } else {
    return false;
  }
}

// Note: This code is taken from Daft-Freak's DaftBoy32
// This creates a flash storage that you can upload the .wasm file to
// Copyright (c) 2020 Charlie Birks - MIT License
// catch running out of memory
#ifdef TARGET_32BLIT_HW
extern "C" void *_sbrk(ptrdiff_t incr) {
  extern char end, __ltdc_start;
  static char *heap_end;

  if (!heap_end)
    heap_end = &end;

  // ltdc is at the end of the heap
  if (heap_end + incr > &__ltdc_start)
    return (void *)-1;

  char *ret = heap_end;
  heap_end += incr;

  return (void *)ret;
}
#endif
void initialize_filesystem() {
#if defined(TARGET_32BLIT_HW)
  extern char _flash_end;
  auto appFilesPtr = &_flash_end;
#elif defined(PICO_BUILD)
  extern char __flash_binary_end;
  auto appFilesPtr = &__flash_binary_end;
  appFilesPtr = (char *)(((uintptr_t)appFilesPtr) + 0xFF &
                         ~0xFF); // round up to 256 byte boundary
#else
  char *appFilesPtr = nullptr;
  return;
#endif

  if (memcmp(appFilesPtr, "APPFILES", 8) != 0)
    return;

  uint32_t numFiles = *reinterpret_cast<uint32_t *>(appFilesPtr + 8);

  const int headerSize = 12, fileHeaderSize = 8;

  auto dataPtr = appFilesPtr + headerSize + fileHeaderSize * numFiles;

  for (auto i = 0u; i < numFiles; i++) {
    auto filenameLength = *reinterpret_cast<uint16_t *>(
        appFilesPtr + headerSize + i * fileHeaderSize);
    auto fileLength = *reinterpret_cast<uint32_t *>(appFilesPtr + headerSize +
                                                    i * fileHeaderSize + 4);

    std::string file_path = "/" + std::string(dataPtr, filenameLength);
    if (endswith(file_path, wasm_extension)) {
      cart_files.push_back(file_path);
    }
    blit::File::add_buffer_file(
        file_path, reinterpret_cast<uint8_t *>(dataPtr + filenameLength),
        fileLength);
    dataPtr += filenameLength + fileLength;
  }
}

void init() {
  blit::set_screen_mode(blit::ScreenMode::hires);
  initialize_filesystem();
  init_apu();
  initialize_wasm4();
#if !defined(TARGET_32BLIT_HW) && !defined(PICO_BUILD)
  cart_files.emplace_back("./cart.wasm");
  for (int i = 1; i < 30; i++) {
    cart_files.emplace_back("./cart.wasm" + std::to_string(i));
  }
#else
  if (cart_files.empty()) {
    auto files = blit::list_files("/");
    for (auto const &file : files) {
      if ((file.flags & blit::FileFlags::directory) == 0) {
        if (endswith(file.name, wasm_extension)) {
          cart_files.push_back(file.name);
        }
      }
    }
  }
#endif
}

void initialize_wasm4() {
  uint8_t *memory = w4_wasmInit();
  w4_runtimeInit(memory, &disk_storage_data);
}

void load_cart(const std::string &cart_file_path) {
  char *cart_bytes;
  size_t cart_length = 0;
#if defined(TARGET_32BLIT_HW) || defined(PICO_BUILD)
  if (cart_file.open(cart_file_path)) {
    cart_length = cart_file.get_length();
    if (cart_length != 0) {
      cart_bytes = static_cast<char *>(malloc(cart_length));
      if (cart_bytes != nullptr) {
        if (cart_file.read(0, cart_length,
                           reinterpret_cast<char *>(cart_bytes)) != -1) {
          // successfully loaded the bytes
          emulator_state = EmulatorState::CART_LOADING;
          w4_wasmLoadModule(reinterpret_cast<const uint8_t *>(cart_bytes),
                            cart_length);
          emulator_state = EmulatorState::CART_LOADED;
        } else {
          // clean cart_bytes as we failed to read to this buffer
          free(cart_bytes);
        }
      }
    }
  }
#else
  FILE *file = fopen(cart_file_path.c_str(), "rb");
  if (file != nullptr) {
    fseek(file, 0, SEEK_END);
    cart_length = ftell(file);
    fseek(file, 0, SEEK_SET);
    cart_bytes = static_cast<char *>(malloc(cart_length));
    if (cart_bytes != nullptr) {
      cart_length = fread(cart_bytes, 1, cart_length, file);
    } else {
      cart_length = 0;
    }
    fclose(file);
    if (cart_length != 0) {
      emulator_state = EmulatorState::CART_LOADING;
      w4_wasmLoadModule(reinterpret_cast<const uint8_t *>(cart_bytes),
                        cart_length);
      emulator_state = EmulatorState::CART_LOADED;
    }
  }
#endif
}

void render(uint32_t time) {
  blit::screen.alpha = 255;
  blit::screen.mask = nullptr;
  blit::screen.pen = blit::Pen(0, 0, 0);
  blit::screen.clear();
  if (emulator_state == EmulatorState::CART_SELECTION) {
    render_selector();
  } else if (emulator_state == EmulatorState::CART_LOADING) {
    blit::screen.pen = blit::Pen(255, 255, 255);
    blit::screen.rectangle(blit::Rect(0, 0, 320, 14));
    blit::screen.pen = blit::Pen(255, 0, 0);
    blit::screen.text("Loading ...", blit::minimal_font, blit::Point(5, 4));
  } else {
    w4_runtimeDraw();
    // White - Red Cursor
    blit::screen.pen = blit::Pen(255, 255, 255);
    int32_t x, y;
    if (get_render() == GpuRenderer::CENTER_RENDER) {
      x = x_center_skip + mouse_x;
      y = y_center_skip + mouse_y;
    } else {
      x = x_skip + static_cast<int32_t>(mouse_x * 1.5);
      y = static_cast<int32_t>(mouse_y * 1.5);
    }
    auto mouse_point = blit::Point{x, y};

    blit::screen.circle(mouse_point, 3);
    blit::screen.pen = blit::Pen(255, 0, 0);
    blit::screen.circle(mouse_point,2);
  }
}

void capture_input() {
  // Player 1 game pad
  uint8_t gamepad = 0;
  if (blit::buttons & blit::Button::X) {
    gamepad |= W4_BUTTON_X;
  }
  if (blit::buttons & blit::Button::Y) {
    gamepad |= W4_BUTTON_Z;
  }
  if (blit::buttons & blit::Button::DPAD_LEFT) {
    gamepad |= W4_BUTTON_LEFT;
  }
  if (blit::buttons & blit::Button::DPAD_RIGHT) {
    gamepad |= W4_BUTTON_RIGHT;
  }
  if (blit::buttons & blit::Button::DPAD_UP) {
    gamepad |= W4_BUTTON_UP;
  }
  if (blit::buttons & blit::Button::DPAD_DOWN) {
    gamepad |= W4_BUTTON_DOWN;
  }
  // Player 1 mouse buttons
  uint8_t mouse_buttons = 0;
  if (blit::buttons & blit::Button::A) {
    mouse_buttons |= W4_MOUSE_LEFT;
  }
  if (blit::buttons & blit::Button::B) {
    mouse_buttons |= W4_MOUSE_RIGHT;
  }
  if (blit::buttons & blit::Button::JOYSTICK) {
    mouse_buttons |= W4_MOUSE_MIDDLE;
  }
  // Player 1 mouse position
  float x_new =
      static_cast<float>(mouse_x) + static_cast<float>(blit::joystick.x) * 3.0f;
  mouse_x = static_cast<int>(x_new);
  if (mouse_x >= 160) {
    mouse_x = 159;
  }
  if (mouse_x < 0) {
    mouse_x = 0;
  }
  float y_new =
      static_cast<float>(mouse_y) + static_cast<float>(blit::joystick.y) * 3.0f;
  mouse_y = static_cast<int>(y_new);
  if (mouse_y < 0) {
    mouse_y = 0;
  }
  if (mouse_y >= 160) {
    mouse_y = 159;
  }
  // Set captured values
  w4_runtimeSetGamepad(0, gamepad);
  w4_runtimeSetGamepad(1, 0); // Disable game pad 2
  w4_runtimeSetMouse(mouse_x, mouse_y, mouse_buttons);
}

void update(uint32_t time) {
  if (emulator_state == EmulatorState::CART_SELECTION) {
    update_selector();
    return;
  }
  capture_input();
  w4_runtimeUpdate();
  play_audio(time, prev_time_ms, first_time);
  if (first_time) {
    first_time = false;
  } else {
    prev_time_ms = time;
  }
}

void render_selector() {
  // Title
  blit::screen.pen = blit::Pen(255, 255, 255);
  blit::screen.rectangle(blit::Rect(0, 0, 320, 14));
  blit::screen.rectangle(blit::Rect(0, TARGET_SIZE - 14, 320, 14));
  blit::screen.pen = blit::Pen(255, 0, 0);
  blit::screen.text("Select wasm4 cart (Press X)", blit::minimal_font, blit::Point(5, 4));
  blit::screen.text(std::string("Render Mode (Press Y to change): ") + (get_render() == GpuRenderer::STRETCH_RENDER ? "1.5x" : "1:1"),
                    blit::minimal_font, blit::Point(5, TARGET_SIZE - 10));
  if (cart_files.empty()) {
    return;
  }
  // Carts list
  clamp_cart_idx();
  if (cart_file_idx < cart_file_render_start) {
    cart_file_render_start--;
  }
  if (cart_file_render_start + cart_file_render_max - 1 < cart_file_idx) {
    cart_file_render_start++;
  }
  for (int i = 0; i < cart_file_render_max; i++) {
    int cur_index = cart_file_render_start + i;
    if (cur_index >= (int)cart_files.size() || cur_index < 0) {
      break;
    }
    if (cur_index == cart_file_idx) {
      blit::screen.pen = blit::Pen(0, 255, 255);
    } else {
      blit::screen.pen = blit::Pen(255, 255, 255);
    }
    blit::screen.text(cart_files[cur_index], blit::minimal_font,
                      blit::Point(10, 20 + i * 20));
  }
}

void update_selector() {
  if (cart_files.empty()) {
    return;
  }
  if (blit::buttons.pressed & blit::Button::DPAD_UP) {
    cart_file_idx--;
    clamp_cart_idx();
  } else if (blit::buttons.pressed & blit::Button::DPAD_DOWN) {
    cart_file_idx++;
    clamp_cart_idx();
  } else if (blit::buttons.pressed & blit::Button::X) {
    std::string cart_file_path = cart_files[cart_file_idx];
    load_cart(cart_file_path);
  } else if (blit::buttons.pressed & blit::Button::Y) {
    if (get_render() == GpuRenderer::CENTER_RENDER) {
      set_render(GpuRenderer::STRETCH_RENDER);
    } else {
      set_render(GpuRenderer::CENTER_RENDER);
    }
  }
}
void clamp_cart_idx() {
  if (cart_file_idx < 0) {
    cart_file_idx = (int)cart_files.size() - 1;
  }
  if (cart_file_idx >= (int)cart_files.size()) {
    cart_file_idx = 0;
  }
}
