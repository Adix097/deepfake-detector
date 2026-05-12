extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/error.h>
    #include <libavutil/samplefmt.h>
}

#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

using namespace std;
const bool RAW_OUT_ON_PLANAR = false;

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
    cout << "Float Output:  " << ((!RAW_OUT_ON_PLANAR || av_sample_fmt_is_planar(codec_ctx->sample_fmt)) ? "yes" : "no") << endl;
}

// IDK this function i copied it from somewhere
float getSample(const AVCodecContext* codecCtx, uint8_t* buffer, int index) {
    switch (av_get_packed_sample_fmt(codecCtx->sample_fmt)) {
        case AV_SAMPLE_FMT_FLT:
            return reinterpret_cast<float*>(buffer)[index];
        case AV_SAMPLE_FMT_DBL:
            return (float)reinterpret_cast<double*>(buffer)[index];
        case AV_SAMPLE_FMT_S16:
            return reinterpret_cast<int16_t*>(buffer)[index] / 32768.0f;
        case AV_SAMPLE_FMT_S32:
            return reinterpret_cast<int32_t*>(buffer)[index] / 2147483648.0f;
        default:
            return 0.0f;
    }
}

void handleFrame(const AVCodecContext* codecCtx, const AVFrame* frame, ofstream& outFile) {
    if(av_sample_fmt_is_planar(codecCtx->sample_fmt) == 1) {
        // This means that the data of each channel is in its own buffer.
        // => frame->extended_data[i] contains data for the i-th channel.
        for(int s = 0; s < frame->nb_samples; ++s) {
            for(int c = 0; c < codecCtx->ch_layout.nb_channels; ++c) {
                float sample = getSample(codecCtx, frame->extended_data[c], s);
                outFile.write(reinterpret_cast<const char*>(&sample), sizeof(float));
            }
        }
    } else {
        // This means that the data of each channel is in the same buffer.
        // => frame->extended_data[0] contains data of all channels.
        if(RAW_OUT_ON_PLANAR) {
            outFile.write(reinterpret_cast<const char*>(frame->extended_data[0]), frame->linesize[0]);
        } else {
            for(int s = 0; s < frame->nb_samples; ++s) {
                for(int c = 0; c < codecCtx->ch_layout.nb_channels; ++c) {
                    float sample = getSample(codecCtx, frame->extended_data[0], s*codecCtx->ch_layout.nb_channels+c);
                    outFile.write(reinterpret_cast<const char*>(&sample), sizeof(float));
                }
            }
        }
    }
}

int receiveAndHandle(AVCodecContext* codec_ctx, AVFrame* frame, ofstream& outFile) {
    int err = 0;
    // Read the packets from the decoder.
    // Each packet may generate more than one frame, depending on the codec
    while((err = avcodec_receive_frame(codec_ctx, frame)) == 0) {
        handleFrame(codec_ctx, frame, outFile);
        av_frame_unref(frame);
    }
    return err;
}

int main() {
    const char* filename = "./input.mp3";

    // making a file to write the output into
    ofstream outFile("output.raw", ios::binary);
    if (!outFile.is_open()) {
        cout << "Failed to create output file!\n";
        return -1;
    }

    int err {0};
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

    // when decoding the decorder will give us an AVFrame containing the decoded data if we give him an AVPacket. The AVPAcket will be given to us by out AVFormatContext.
    AVFrame* frame {nullptr};

    if (!(frame = av_frame_alloc())) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }

    // prepare the packet
    AVPacket* packet = av_packet_alloc();

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
        if ((err = receiveAndHandle(codec_ctx, frame, outFile)) != AVERROR(EAGAIN)) {
            printError("Something went wrong while receiving decoded frames!", err);
            break;
        }
    }

    avcodec_send_packet(codec_ctx, nullptr);
    receiveAndHandle(codec_ctx, frame, outFile);

    // at last clean up everything we opened.
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}