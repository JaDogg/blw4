#include "game.hpp"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

extern "C" {
#include "src/runtime.h"
#include "src/wasm.h"
}

static w4_Disk disk_storage_data{0, {}};
static bool cart_loaded = false;
static bool first_time = true;
static uint32_t prev_time_ms = 0;
double fps = 0.0;
std::stringstream temp_sstream;

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
blit::File cart_file{};

void init() {
  blit::set_screen_mode(blit::ScreenMode::hires);
  initialize_filesystem();
  blit::channels[0].waveforms = blit::Waveform::SQUARE;
  blit::channels[1].waveforms = blit::Waveform::SQUARE;
  blit::channels[2].waveforms = blit::Waveform::TRIANGLE;
  blit::channels[3].waveforms = blit::Waveform::NOISE;
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
    blit::screen.pen = blit::Pen(255, 255, 255);
    blit::screen.rectangle(blit::Rect(0, 160, 320, 14));
    blit::screen.pen = blit::Pen(255, 0, 0);
    blit::screen.text("Running: cart.wasm", blit::minimal_font,
                      blit::Point(5, 164));
    blit::screen.pen = blit::Pen(255, 0, 0);
    temp_sstream.str("");
    temp_sstream.clear();
    temp_sstream << "FPS:  " << std::fixed << std::setprecision(2) << fps;
    blit::screen.text(temp_sstream.str(), blit::minimal_font,
                      blit::Point(160, 165));
  }
}

uint8_t* rgb(uint8_t* target, uint32_t colour) {
  uint8_t r = (((colour & 0xff0000) >> 8) >> 8);
  uint8_t g = ((colour & 0xff00) >> 8);
  uint8_t b = (colour & 0xff);
  *target++ = r;
  *target++ = g;
  *target++ = b;
  return target;
}

extern "C" {
void w4_windowComposite(const uint32_t *palette, const uint8_t *framebuffer) {
  int pixel = -1;
  for (int n = 0; n < 160 * 160 / 4; ++n) {
    uint8_t quartet = framebuffer[n];
    int c1 = (quartet & 0b00000011) >> 0;
    int c2 = (quartet & 0b00001100) >> 2;
    int c3 = (quartet & 0b00110000) >> 4;
    int c4 = (quartet & 0b11000000) >> 6;

    pixel++;
    int y = pixel / 160;
    int x = pixel % 160;
    rgb(blit::screen.ptr(x, y), palette[c1]);

    pixel++;
    y = pixel / 160;
    x = pixel % 160;
    rgb(blit::screen.ptr(x, y), palette[c2]);

    pixel++;
    y = pixel / 160;
    x = pixel % 160;
    rgb(blit::screen.ptr(x, y), palette[c3]);

    pixel++;
    y = pixel / 160;
    x = pixel % 160;
    rgb(blit::screen.ptr(x, y), palette[c4]);

  }
}
}


int g_min(int x, int y) { return x < y ? x : y; }
int g_max(int x, int y) { return x > y ? x : y; }

enum class AUDIO_STEP {
  N /*Nothing*/,
  A_START,
  A_HOLD,
  D_START,
  D_HOLD,
  S_START,
  S_HOLD,
  R_START,
  R_HOLD
};
int attack_ms[4] = {0, 0, 0, 0};
int decay_ms[4] = {0, 0, 0, 0};
int sustain_ms[4] = {0, 0, 0, 0};
int release_ms[4] = {0, 0, 0, 0};
AUDIO_STEP audio_task[4] = {AUDIO_STEP::N, AUDIO_STEP::N, AUDIO_STEP::N,
                            AUDIO_STEP::N};

extern "C" {
void wasm4_tone_callback(uint32_t frequency, uint32_t duration, uint32_t volume,
                         uint32_t flags) {
  int freq1 = frequency & 0xffff;
  int freq2 = (frequency >> 16) & 0xffff;

  int sustain = duration & 0xff;
  int release = (duration >> 8) & 0xff;
  int decay = (duration >> 16) & 0xff;
  int attack = (duration >> 24) & 0xff;

  int sustainVolume = g_min(volume & 0xff, 100);
  int peakVolume = g_min((volume >> 8) & 0xff, 100);

  int channelIdx = flags & 0x03;
  int mode = (flags >> 2) & 0x3; // TODO
  int pan = (flags >> 4) & 0x3;  // Cannot be done unless we support stereo
  blit::channels[channelIdx].frequency = freq1;
  blit::channels[channelIdx].filter_cutoff_frequency = freq2;
  blit::channels[channelIdx].sustain = 0xffff; // * sustainVolume / 100;
  blit::channels[channelIdx].volume = 0xffff;  //* peakVolume / 100;
  blit::channels[channelIdx].attack_ms = g_max(attack * 1000 / fps, 1);
  blit::channels[channelIdx].decay_ms = g_max(decay * 1000 / fps, 1);
  blit::channels[channelIdx].release_ms = g_max(release * 1000 / fps, 1);
  attack_ms[channelIdx] = g_max(attack * 1000 / fps, 1);
  decay_ms[channelIdx] = g_max(decay * 1000 / fps, 1);
  sustain_ms[channelIdx] = g_max(sustain * 1000 / fps, 1);
  release_ms[channelIdx] = g_max(release * 1000 / fps, 1);
  audio_task[channelIdx] = AUDIO_STEP::A_START;
}
}

void play_audio(uint32_t time_ms) {
  int delta = 0;
  if (first_time) {
    first_time = false;
  } else {
    delta = time_ms - prev_time_ms;
    prev_time_ms = time_ms;
  }
  for (int chan = 0; chan < 4; chan++) {
    if (audio_task[chan] == AUDIO_STEP::N) {
      continue;
    }
    // ATTACK
    if (audio_task[chan] == AUDIO_STEP::A_START) {
      if (attack_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::D_START;
      } else {
        audio_task[chan] = AUDIO_STEP::A_HOLD;
        blit::channels[chan].trigger_attack();
      }
      continue;
    }
    if (audio_task[chan] == AUDIO_STEP::A_HOLD) {
      attack_ms[chan] -= delta;
      if (attack_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::D_START;
      }
    }
    // DECAY
    if (audio_task[chan] == AUDIO_STEP::D_START) {
      if (decay_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::S_START;
      } else {
        audio_task[chan] = AUDIO_STEP::D_HOLD;
        blit::channels[chan].trigger_decay();
      }
      continue;
    }
    if (audio_task[chan] == AUDIO_STEP::D_HOLD) {
      decay_ms[chan] -= delta;
      if (decay_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::S_START;
      }
    }
    // SUSTAIN
    if (audio_task[chan] == AUDIO_STEP::S_START) {
      if (sustain_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::R_START;
      } else {
        audio_task[chan] = AUDIO_STEP::S_HOLD;
        blit::channels[chan].trigger_sustain();
      }
      continue;
    }
    if (audio_task[chan] == AUDIO_STEP::S_HOLD) {
      sustain_ms[chan] -= delta;
      if (sustain_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::R_START;
      }
    }
    // RELEASE
    if (audio_task[chan] == AUDIO_STEP::R_START) {
      if (release_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::N;
        blit::channels[chan].off();
      } else {
        audio_task[chan] = AUDIO_STEP::R_HOLD;
        blit::channels[chan].trigger_release();
      }
      continue;
    }
    if (audio_task[chan] == AUDIO_STEP::R_HOLD) {
      release_ms[chan] -= delta;
      if (release_ms[chan] <= 0) {
        audio_task[chan] = AUDIO_STEP::N;
        blit::channels[chan].off();
      }
    }
  }
}

void capture_input() { // Set player 1 gamepad
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
  w4_runtimeSetGamepad(0, gamepad);
  // Gamepad2 is not supported
  w4_runtimeSetGamepad(1, 0);
  // TODO see if we can emulate mouse with A + arrows, B -> left, A + B ->
  // Right, A + Y -> Middle
  w4_runtimeSetMouse(0, 0, 0);
}

void update(uint32_t time) {
  if (!cart_loaded) {
    return;
  }
  capture_input();
  if (!first_time) {
    float delta_ms = time - prev_time_ms;
    fps = (1.0 / static_cast<double>(delta_ms)) * 1000.0;
  }
  w4_runtimeUpdate();
  play_audio(time);
}
