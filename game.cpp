#include "32blit.hpp"
#include "src/apu.hpp"
#include <cstring>
#include <iostream>

extern "C" {
#include "src/runtime.h"
#include "src/wasm.h"
}

#define TARGET_SIZE 240
#if defined(PICO_BUILD)
#define TARGET_WIDTH 240
#else
#define TARGET_WIDTH 320
#endif
static const int x_skip = (TARGET_WIDTH - TARGET_SIZE) / 2;
static w4_Disk disk_storage_data{0, {}};
static bool cart_loaded = false;
static bool first_time = true;
static uint32_t prev_time_ms = 0;
static int mouse_x = 80;
static int mouse_y = 80;

blit::File cart_file{};

// Note: This code is taken from Daft-Freak's DaftBoy32
// This creates a flash storage that you can upload the .wasm file to
// Copyright (c) 2020 Charlie Birks - MIT License
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

    blit::File::add_buffer_file(
        "/" + std::string(dataPtr, filenameLength),
        reinterpret_cast<uint8_t *>(dataPtr + filenameLength), fileLength);

    dataPtr += filenameLength + fileLength;
  }
}

void init() {
  blit::set_screen_mode(blit::ScreenMode::hires);
  initialize_filesystem();
  init_apu();
  char *cart_bytes;
  size_t cart_length = 0;
  uint8_t *memory = w4_wasmInit();
  w4_runtimeInit(memory, &disk_storage_data);
#if defined(TARGET_32BLIT_HW) || defined(PICO_BUILD)
  if (cart_file.open("cart.wasm")) {
    cart_length = cart_file.get_length();
    if (cart_length != 0) {
      cart_bytes = static_cast<char *>(malloc(cart_length));
      if (cart_bytes != nullptr) {
        if (cart_file.read(0, cart_length, reinterpret_cast<char *>(cart_bytes)) !=
            -1) {
          // successfully loaded the bytes
          w4_wasmLoadModule(reinterpret_cast<const uint8_t *>(cart_bytes),
                            cart_length);
          cart_loaded = true;
        } else {
          // clean cart_bytes as we failed to read to this buffer
          free(cart_bytes);
        }
      }
    }
  }
#else
  FILE *file = fopen("./cart.wasm", "rb");
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
      w4_wasmLoadModule(reinterpret_cast<const uint8_t *>(cart_bytes),
                        cart_length);
      cart_loaded = true;
    }
  }
#endif
}

void render(uint32_t time) {
  blit::screen.alpha = 255;
  blit::screen.mask = nullptr;
  blit::screen.pen = blit::Pen(0, 0, 0);
  blit::screen.clear();

  if (!cart_loaded) {
    blit::screen.pen = blit::Pen(255, 255, 255);
    blit::screen.rectangle(blit::Rect(0, 0, 320, 14));
    blit::screen.pen = blit::Pen(255, 0, 0);
    blit::screen.text("Failed to load: cart.wasm", blit::minimal_font,
                      blit::Point(5, 4));
  } else {
    w4_runtimeDraw();
    // White - Red Cursor
    blit::screen.pen = blit::Pen(255, 255, 255);
    blit::screen.circle(blit::Point{x_skip + static_cast<int32_t>(mouse_x * 1.5), static_cast<int32_t>(mouse_y * 1.5)}, 3);
    blit::screen.pen = blit::Pen(255, 0, 0);
    blit::screen.circle(blit::Point{x_skip + static_cast<int32_t>(mouse_x * 1.5), static_cast<int32_t>(mouse_y * 1.5)}, 2);
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
  float x_new = static_cast<float>(mouse_x) + static_cast<float>(blit::joystick.x) * 3.0f;
  mouse_x = static_cast<int>(x_new);
  if (mouse_x >= 160) {
    mouse_x = 159;
  }
  if (mouse_x < 0) {
    mouse_x = 0;
  }
  float y_new = static_cast<float>(mouse_y) + static_cast<float>(blit::joystick.y) * 3.0f;
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
  capture_input();
  w4_runtimeUpdate();
  play_audio(time, prev_time_ms, first_time);
  if (first_time) {
    first_time = false;
  } else {
    prev_time_ms = time;
  }
}
