#include "audio/decoder.hpp"
#include "dsp/stft.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <filesystem>

using namespace std;

static const int TARGET_SAMPLE_RATE = 16000;

int main() {
    const string filename = "./input/input.mp3";

    // decode
    vector<float> samples;
    try {
        samples = decodeAudioFile(filename);
    } catch (const runtime_error& e) {
        cerr << e.what() << endl;
        return -1;
    }

    // normalize
    float peak = 0.0f;
    for (float s : samples) peak = max(peak, fabsf(s));
    cout << "Peak amplitude: " << peak << endl;

    if (peak > 0.0f) {
        const float scale = 1.0f / peak;
        for (float& s : samples) s *= scale;
    }

    // STFT
    auto spectrogram = computeSTFT(samples, TARGET_SAMPLE_RATE);

    // write CSV
    std::filesystem::path dir = "./output";
    if (!std::filesystem::exists(dir)) std::filesystem::create_directories(dir);
    std::ofstream specFile(dir / "spectrogram.csv");
    if (!specFile.is_open()) std::cerr << "Failed to open file\n";

    for (size_t t = 0; t < spectrogram.size(); ++t) {
        const auto& mags = spectrogram[t].magnitudes;
        for (size_t f = 0; f < mags.size(); ++f) {
            specFile << mags[f];
            if (f + 1 < mags.size()) specFile << ',';
        }
        specFile << '\n';
    }
    specFile.close();

    cout << "Wrote " << spectrogram.size() << " frames to spectrogram.csv" << endl;
    cout << "Done\n";
    return 0;
}