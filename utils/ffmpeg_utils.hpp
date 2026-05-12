#pragma once

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

int  printError(const char* prefix, int errorCode);
int  findAudioStream(const AVFormatContext* format_ctx);
void printStreamInformation(const AVCodec* codec,
                            const AVCodecContext* codec_ctx,
                            int audioStreamIndex);