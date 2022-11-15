#pragma once
#include <cstdint>
void play_audio(uint32_t time_ms, uint32_t prev_time_ms, bool first_time);
void init_apu();
double get_fps();
