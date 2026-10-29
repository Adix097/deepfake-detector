# Deepfake Detector

Audio preprocessing for a deepfake-detection pipeline, written in C++.
The current implementation decodes an audio file, converts it to 16 kHz mono
Float32 samples, normalizes the signal, computes a Short-Time Fourier Transform
(STFT), and writes the resulting magnitude spectrogram to a CSV file.

> **Current status:** this repository contains feature extraction only. It does
> not yet include a trained deepfake-classification model or prediction step.

## Features

- Decodes audio formats supported by FFmpeg.
- Resamples audio to 16 kHz mono Float32.
- Normalizes samples by peak amplitude.
- Computes STFT features with a 25 ms Hann window and 10 ms hop.
- Exports one spectrogram frame per CSV row.

## Requirements

On Ubuntu/Debian, install the development dependencies with:

```bash
sudo apt update
sudo apt install build-essential pkg-config \
  libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libfftw3-dev
```

The project requires a C++ compiler, FFmpeg development libraries, FFTW3, and
`pkg-config`.

## Build

From the repository root:

```bash
chmod +x build.sh
./build.sh
```

This creates the executable `app.out`.

## Usage

The executable currently reads a fixed input path:

```text
input/input.mp3
```

Create the input directory, copy an audio file into it, and run:

```bash
mkdir -p input
cp /path/to/audio.mp3 input/input.mp3
./app.out
```

The generated spectrogram is written to:

```text
output/spectrogram.csv
```

Each row represents one STFT frame, and each comma-separated value is the
magnitude of one frequency bin.

## Project layout

```text
.
├── audio/
│   ├── decoder.cpp       # FFmpeg decoding and resampling
│   └── decoder.hpp
├── dsp/
│   ├── stft.cpp          # Hann-windowed STFT implementation
│   └── stft.hpp
├── utils/
│   ├── ffmpeg_utils.cpp  # FFmpeg helpers and error reporting
│   └── ffmpeg_utils.hpp
├── build.sh
└── main.cpp
```

## Limitations

- The input filename is currently hard-coded in `main.cpp`.
- The program processes audio only; video streams are ignored.
- No classifier, model training code, or deepfake score is included yet.
- Audio shorter than one STFT window produces no spectrogram frames.

## Roadmap

- Accept input and output paths from command-line arguments.
- Add feature scaling and metadata alongside the CSV output.
- Integrate a trained deepfake audio classifier.
- Add automated tests and example fixtures.

## License

No license has been specified yet.
