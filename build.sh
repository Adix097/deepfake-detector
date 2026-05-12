#!/bin/bash
g++ main.cpp audio/decoder.cpp dsp/stft.cpp utils/ffmpeg_utils.cpp -o app.out $(pkg-config --cflags --libs libavformat libavcodec libavutil libswresample fftw3)