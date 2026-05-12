extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/error.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h>
}

#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <cmath>

using namespace std;

// helper function just to print any errors
int printError(const char* prefix, int errorCode) {
    if (errorCode == 0) return 0;
    else {
        const size_t buffer_size = 64;
        char buffer[buffer_size];

        if (av_strerror(errorCode, buffer, buffer_size) != 0) strcpy(buffer, "Unknown error!");
        cout << prefix << " (" << errorCode << ":" << buffer << ")" << endl;
        
        return errorCode;
    }
}

// to find audio stream, just iterate over the streams and check for stream type
int findAudioStream(const AVFormatContext* format_ctx) {
    int audioStreamIndex = -1;
    for(size_t i = 0; i < format_ctx->nb_streams; ++i) {
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }
    return audioStreamIndex;
}

void printStreamInformation(const AVCodec* codec, const AVCodecContext* codec_ctx, int audioStreamIndex) {
    cout << "Codec: " << codec->long_name << endl;
    const AVSampleFormat* sample_fmts = nullptr;
    if (avcodec_get_supported_config(
        nullptr, 
        codec, 
        AV_CODEC_CONFIG_SAMPLE_FORMAT, 
        0, 
        (const void**)&sample_fmts, 
        nullptr) == 0
        && sample_fmts != nullptr) {
            cout << "Supported sample formats: ";
            for (int i = 0; sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
            if (i > 0) cout << ", ";
            cout << av_get_sample_fmt_name(sample_fmts[i]);
        }
        cout << endl;
    }
    cout << endl;
    cout << "Stream:        " << audioStreamIndex << endl;
    cout << "Sample Format: " << av_get_sample_fmt_name(codec_ctx->sample_fmt) << endl;
    cout << "Sample Rate:   " << codec_ctx->sample_rate << endl;
    cout << "Sample Size:   " << av_get_bytes_per_sample(codec_ctx->sample_fmt) << endl;
    cout << "Channels:      " << codec_ctx->ch_layout.nb_channels << endl;
    cout << "Float Output:  " << ((av_sample_fmt_is_planar(codec_ctx->sample_fmt)) ? "yes" : "no") << endl;
}

int main() {
    const char* filename = "./input.mp3";
    const int TARGET_SAMPLE_RATE {16000};
    int err {0};

    // Open file and find stream
    AVFormatContext* format_ctx {nullptr};
    if ((err = avformat_open_input(&format_ctx, filename, nullptr, 0)) != 0) {
        return printError("Error opening file!", err);
    }

    // find the audio stream of the input file
    int audioStreamIndex = findAudioStream(format_ctx);
    if (audioStreamIndex == -1) {
        cout << "None of the available streams are audio streams\n";
        avformat_close_input(&format_ctx);
        return -1;
    }

    // find the correct decoder for the codec
    const AVCodec* codec = avcodec_find_decoder(format_ctx->streams[audioStreamIndex]->codecpar->codec_id);
    if (!codec) {
        cout << "Decoder was not found. The codec is not supported.\n";
        avformat_close_input(&format_ctx);
        return -1;
    }

    // initialize the codec context for the decoder
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        avformat_close_input(&format_ctx);
        cout << "Could not allocate a decoding context.\n";
        return -1;
    }

    if ((err = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[audioStreamIndex]->codecpar)) != 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return printError("Error setting codec context parameters.", err);
    }

    // Add this after avcodec_parameters_to_context succeeds:
    if ((err = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return printError("Failed to open codec.", err);
    }

    // setup resampler,
    // Input: whatever the decoder gives
    // Output: 16Khz, mono, float32

    SwrContext* swr_ctx {nullptr};
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    AVChannelLayout in_ch_layout = codec_ctx->ch_layout;

    if ((err = swr_alloc_set_opts2(
        &swr_ctx,
        &out_ch_layout,
        AV_SAMPLE_FMT_FLT,
        TARGET_SAMPLE_RATE,
        &in_ch_layout,
        codec_ctx->sample_fmt,
        codec_ctx->sample_rate,
        0, nullptr)) < 0) {
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&format_ctx);
            return printError("Failed to setup resampler.", err);
    }

    if ((err = swr_init(swr_ctx)) < 0) {
        swr_free(&swr_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return printError("Failed to initialize resampler.", err);
    }

    // decode & resample into memory
    // when decoding the decorder will give us an AVFrame containing the decoded data if we give him an AVPacket. The AVPAcket will be given to us by out AVFormatContext.
    
    vector<float> samples;
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();

    auto processFrame = [&](AVFrame* f) {
        int64_t out_count = av_rescale_rnd(
            swr_get_delay(swr_ctx, codec_ctx->sample_rate) + f->nb_samples,
            TARGET_SAMPLE_RATE,
            codec_ctx->sample_rate,
            AV_ROUND_UP
        );

        vector<float> buff(out_count);
        uint8_t* out_data = reinterpret_cast<uint8_t*>(buff.data());

        int converted = swr_convert (
            swr_ctx,
            &out_data,
            out_count,
            (const uint8_t**)f->extended_data,
            f->nb_samples
        );

        if (converted > 0) {
            buff.resize(converted);
            samples.insert(samples.end(), buff.begin(), buff.end());
        }
    };

    auto receiveAndProcess = [&]() -> int {
        int e = 0;
        while ((e = avcodec_receive_frame(codec_ctx, frame)) == 0) {
            processFrame(frame);
            av_frame_unref(frame);
        }
        return e;
    };

    // read the file
    while ((err = av_read_frame(format_ctx, packet)) != AVERROR_EOF) {
        
        if (err != 0) {
            printError("Something went wrong!", err);
            break;
        }

        // does the packet belong to correct stream?
        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if ((err = avcodec_send_packet(codec_ctx, packet)) == 0) {
            // the packet was sent successfully we do not need it anymore unref the buffer used by the frame
            av_packet_unref(packet);
        } else {
            printError("Something went wrong!", err);
            break;
        }

        // sometimes while receiving frames error code AVERROR(EAGAIN) hits, which simply means "all buffers are full, receive some frames and try again" so it is not an actual error.
        if ((err = receiveAndProcess()) != AVERROR(EAGAIN)) {
            printError("Something went wrong while receiving decoded frames!", err);
            break;
        }
    }

    avcodec_send_packet(codec_ctx, nullptr);
    receiveAndProcess();

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

    // Normalize
    float peak = 0.0f;
    for (auto s : samples) peak = max(peak, abs(s));
    cout << "Peak amplitude: " << peak << endl;

    if (peak > 0.0f) {
        auto scale = 1.0f / peak;
        for (auto& s : samples) s *= scale;
    }

    // at last clean up everything we opened.
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}