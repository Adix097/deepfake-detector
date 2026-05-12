#include "decoder.hpp"
#include "../utils/ffmpeg_utils.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h>
}

#include <iostream>
#include <stdexcept>
#include <cstring>

using namespace std;

static const int TARGET_SAMPLE_RATE = 16000;

static void processFrame(AVFrame* f, SwrContext* swr_ctx, AVCodecContext* codec_ctx, vector<float>& samples) {
    int64_t out_count = av_rescale_rnd(
        swr_get_delay(swr_ctx, codec_ctx->sample_rate) + f->nb_samples,
        TARGET_SAMPLE_RATE,
        codec_ctx->sample_rate,
        AV_ROUND_UP);

    vector<float> buff(out_count);
    uint8_t* out_data = reinterpret_cast<uint8_t*>(buff.data());

    int converted = swr_convert(swr_ctx, &out_data, out_count, (const uint8_t**)f->extended_data, f->nb_samples);
    if (converted > 0) {
        buff.resize(converted);
        samples.insert(samples.end(), buff.begin(), buff.end());
    }
}

static int receiveAndProcess(AVFrame* f, SwrContext* swr_ctx, AVCodecContext* codec_ctx, vector<float>& samples) {
    int err {0};
    while ((err = avcodec_receive_frame(codec_ctx, f)) == 0) {
        processFrame(f, swr_ctx, codec_ctx, samples);
        av_frame_unref(f);
    }
    return err;
}

vector<float> decodeAudioFile(const string& filename) {
    int err = 0;

    AVFormatContext* format_ctx = nullptr;
    if ((err = avformat_open_input(&format_ctx, filename.c_str(), nullptr, 0)) != 0) {
        printError("Error opening file!", err);
        throw runtime_error("avformat_open_input failed");
    }

    if ((err = avformat_find_stream_info(format_ctx, nullptr)) < 0) {
        avformat_close_input(&format_ctx);
        printError("Failed to find stream info.", err);
        throw runtime_error("avformat_find_stream_info failed");
    }

    // locate audio stream
    int audioStreamIndex = findAudioStream(format_ctx);
    if (audioStreamIndex == -1) {
        avformat_close_input(&format_ctx);
        throw runtime_error("No audio stream found in file");
    }

    // setup decoder
    const AVCodec* codec = avcodec_find_decoder(format_ctx->streams[audioStreamIndex]->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&format_ctx);
        throw runtime_error("Decoder not found");
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        avformat_close_input(&format_ctx);
        throw runtime_error("Could not initialize decoder");
    }

    if ((err = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[audioStreamIndex]->codecpar)) != 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        printError("Unable to set context parameters.", err);
        throw runtime_error("avcodec_parameters_to_context failed");
    }

    if ((err = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        printError("Failed to open codec.", err);
        throw runtime_error("avcodec_open2 failed");
    }

    // set up resampler: 16 kHz, mono, float32
    SwrContext* swr_ctx = nullptr;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    AVChannelLayout in_ch_layout  = codec_ctx->ch_layout;

    if ((err = swr_alloc_set_opts2(
        &swr_ctx,
        &out_ch_layout, AV_SAMPLE_FMT_FLT, TARGET_SAMPLE_RATE,
        &in_ch_layout,  codec_ctx->sample_fmt, codec_ctx->sample_rate,
        0, nullptr)) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        printError("Failed to setup resampler.", err);
        throw runtime_error("swr_alloc_set_opts2 failed");
    }

    if ((err = swr_init(swr_ctx)) < 0) {
        swr_free(&swr_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        printError("Failed to initialize resampler.", err);
        throw runtime_error("swr_init failed");
    }

    // decode loop
    vector<float> samples;
    AVFrame* frame  = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();

    while ((err = av_read_frame(format_ctx, packet)) != AVERROR_EOF) {
        if (err != 0) {
            printError("Error reading frame!", err);
            break;
        }

        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if ((err = avcodec_send_packet(codec_ctx, packet)) == 0) {
            av_packet_unref(packet);
        } else {
            printError("Error sending packet!", err);
            break;
        }

        // sometimes while receiving frames error code AVERROR(EAGAIN) hits, which simply means "all buffers are full, receive some frames and try again" so it is not an actual error.
        if ((err = receiveAndProcess(frame, swr_ctx, codec_ctx, samples)) != AVERROR(EAGAIN)) {
            printError("Error receiving decoded frames!", err);
            break;
        }
    }

    // cleanup
    avcodec_send_packet(codec_ctx, nullptr);
    receiveAndProcess(frame, swr_ctx, codec_ctx, samples);

    int64_t remaining = swr_get_delay(swr_ctx, codec_ctx->sample_rate);
    if (remaining > 0) {
        vector<float> buff(remaining);
        uint8_t* out_data = reinterpret_cast<uint8_t*>(buff.data());
        int converted = swr_convert(swr_ctx, &out_data, remaining, nullptr, 0);
        if (converted > 0) {
            buff.resize(converted);
            samples.insert(samples.end(), buff.begin(), buff.end());
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return samples;
}