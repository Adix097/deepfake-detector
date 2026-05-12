#pragma once

#include <vector>

struct SpectrogramFrame {
    std::vector<float> magnitudes;
};

// Compute a Short-Time Fourier Transform (STFT) over the given samples.
std::vector<SpectrogramFrame> computeSTFT(const std::vector<float>& samples, int sample_rate, int window_ms = 25, int hop_ms = 10);