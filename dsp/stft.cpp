#include "stft.hpp"

#include <fftw3.h>
#include <cmath>
#include <vector>

using namespace std;

vector<SpectrogramFrame> computeSTFT(const vector<float>& samples, int sample_rate, int window_ms, int hop_ms) {
    const int window_size = sample_rate * window_ms / 1000;
    const int hop_size = sample_rate * hop_ms    / 1000;
    const int fft_bins = window_size / 2 + 1;

    // Pre-compute Hann window
    vector<float> hann(window_size);
    for (int i = 0; i < window_size; ++i) hann[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (window_size - 1)));

    // FFTW buffers and plan (created once, reused every frame)
    vector<double> in(window_size);
    vector<fftw_complex> out(fft_bins);
    fftw_plan plan = fftw_plan_dft_r2c_1d(window_size, in.data(), out.data(), FFTW_ESTIMATE);

    vector<SpectrogramFrame> spectrogram;

    for (int start = 0; start + window_size <= static_cast<int>(samples.size()); start += hop_size) {
        // Apply Hann window
        for (int i = 0; i < window_size; ++i) in[i] = samples[start + i] * hann[i];
        fftw_execute(plan);

        SpectrogramFrame frame;
        frame.magnitudes.resize(fft_bins);
        for (int i = 0; i < fft_bins; ++i) {
            float re = static_cast<float>(out[i][0]);
            float im = static_cast<float>(out[i][1]);
            frame.magnitudes[i] = sqrtf(re * re + im * im);
        }

        spectrogram.push_back(move(frame));
    }

    fftw_destroy_plan(plan);
    return spectrogram;
}