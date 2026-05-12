#pragma once

#include <vector>
#include <string>

// Opens an audio file, decodes it, resamples to 16 kHz mono float32,
// and returns the raw sample buffer.
std::vector<float> decodeAudioFile(const std::string& filename);