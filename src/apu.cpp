#include "32blit.hpp"

static double fps = 0.0;

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

static int attack_ms[4] = {0, 0, 0, 0};
static int decay_ms[4] = {0, 0, 0, 0};
static int sustain_ms[4] = {0, 0, 0, 0};
static int release_ms[4] = {0, 0, 0, 0};
static AUDIO_STEP audio_task[4] = {AUDIO_STEP::N, AUDIO_STEP::N, AUDIO_STEP::N,
                                   AUDIO_STEP::N};

int g_min(int x, int y) { return x < y ? x : y; }
int g_max(int x, int y) { return x > y ? x : y; }

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

void init_apu() {
  blit::channels[0].waveforms = blit::Waveform::SQUARE;
  blit::channels[1].waveforms = blit::Waveform::SQUARE;
  blit::channels[2].waveforms = blit::Waveform::TRIANGLE;
  blit::channels[3].waveforms = blit::Waveform::NOISE;
}

double get_fps() { return fps; }

void play_audio(uint32_t time_ms, uint32_t prev_time_ms, bool first_time) {
  int delta = 0;
  if (!first_time) {
    delta = time_ms - prev_time_ms;
    fps = (1.0 / delta) * 1000.0;
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
