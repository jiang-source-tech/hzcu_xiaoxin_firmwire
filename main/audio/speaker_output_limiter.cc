#include "speaker_output_limiter.h"

#include <algorithm>
#include <cmath>

int16_t speaker_output_limiter_peak_from_db(float ceiling_db) {
    if (ceiling_db >= 0.0f) {
        return INT16_MAX;
    }

    float linear = std::pow(10.0f, ceiling_db / 20.0f);
    int peak = static_cast<int>(std::lround(static_cast<float>(INT16_MAX) * linear));
    peak = std::clamp(peak, 1, static_cast<int>(INT16_MAX));
    return static_cast<int16_t>(peak);
}

void speaker_output_limiter_apply(std::vector<int16_t>& pcm, int16_t peak) {
    int16_t safe_peak = peak > 0 ? peak : INT16_MAX;
    for (auto& sample : pcm) {
        if (sample > safe_peak) {
            sample = safe_peak;
        } else if (sample < -safe_peak) {
            sample = static_cast<int16_t>(-safe_peak);
        }
    }
}
