#pragma once

#include <stdbool.h>

#include "render.h"

bool gpu_bloom_init(void);
void gpu_bloom_shutdown(void);

void gpu_bloom_set_parameters(bool enabled,
                              int radius,
                              int intensity,
                              int quad_passes);

bool gpu_bloom_present(Surface *top,
                       Surface *bottom,
                       bool bottom_changed,
                       bool stereo,
                       int stereo_shift);
