#include "ffmpeg_utils.hpp"

extern "C" {
    #include <libavutil/error.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/channel_layout.h>
}

#include <iostream>
#include <cstring>

using namespace std;

int printError(const char* prefix, int errorCode) {
    if (errorCode == 0) return 0;

    const size_t buffer_size = 64;
    char buffer[buffer_size];

    if (av_strerror(errorCode, buffer, buffer_size) != 0)
        strcpy(buffer, "Unknown error!");

    cout << prefix << " (" << errorCode << ":" << buffer << ")" << endl;
    return errorCode;
}

int findAudioStream(const AVFormatContext* format_ctx) {
    for (size_t i = 0; i < format_ctx->nb_streams; ++i) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) return static_cast<int>(i);
    }
    return -1;
}

void printStreamInformation(const AVCodec* codec, const AVCodecContext* codec_ctx, int audioStreamIndex) {
    cout << "Codec: " << codec->long_name << endl;

    const AVSampleFormat* sample_fmts = nullptr;
    if (avcodec_get_supported_config(
            nullptr, codec,
            AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
            (const void**)&sample_fmts, nullptr) == 0
        && sample_fmts != nullptr) {
        cout << "Supported sample formats: ";
        for (int i = 0; sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
            if (i > 0) cout << ", ";
            cout << av_get_sample_fmt_name(sample_fmts[i]);
        }
        cout << endl;
    }

    cout << "\nStream:        " << audioStreamIndex                                   << endl;
    cout << "Sample Format: " << av_get_sample_fmt_name(codec_ctx->sample_fmt)        << endl;
    cout << "Sample Rate:   " << codec_ctx->sample_rate                               << endl;
    cout << "Sample Size:   " << av_get_bytes_per_sample(codec_ctx->sample_fmt)       << endl;
    cout << "Channels:      " << codec_ctx->ch_layout.nb_channels                     << endl;
    cout << "Float Output:  " << (av_sample_fmt_is_planar(codec_ctx->sample_fmt) ? "yes" : "no") << endl;
}