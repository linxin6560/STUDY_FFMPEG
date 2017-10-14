//
// Created by Administrator on 2017/10/12.
//

#include "FFMpegMusic.h"

AVFormatContext *formatContext;
//获取上下文
AVCodecContext *audioContext;
//获取解码器
AVCodec *codec;
AVPacket *pPacket;
AVFrame *frame;
SwrContext *swrContext;
uint8_t *out_buffer;
int out_channel_nb;

int createFFmpeg(const char *input, int *rate, int *channel) {
    LOGE("input=%s", input);
    av_register_all();//ffmpeg代码必须调用
    formatContext = avformat_alloc_context();
    int result = avformat_open_input(&formatContext, input, NULL, NULL);
    if (result < 0) {
        LOGE("音频打开失败:%d", result);
        return result;
    }
    result = avformat_find_stream_info(formatContext, NULL);
    if (result < 0) {
        LOGE("找寻流信息失败:%d", result);
        return result;
    }
    //寻找流信息
    int audio_stream_index = -1;
    for (int i = 0; i < (formatContext)->nb_streams; ++i) {
        LOGE("循环 %d", i);
        //特别注意，这里的codec_type千万别打成coder_type
        if ((formatContext)->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }
    LOGE("audio_stream_index=%d", audio_stream_index);
    if (audio_stream_index == -1) {
        LOGE("找寻视频流信息失败");
        return -1;
    }
    //获取上下文
    audioContext = formatContext->streams[audio_stream_index]->codec;
    LOGE("获取上下文成功,宽 %d  高 %d", audioContext->width, audioContext->height);
    //获取解码器
    codec = avcodec_find_decoder(audioContext->codec_id);
    LOGE("获取解码器成功");
    result = avcodec_open2(audioContext, codec, NULL);
    if (result < 0) {
        LOGE("解码失败:%d", result);
        return result;
    }
    pPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(pPacket);
    LOGE("创建并初始化AVPacket成功");
    swrContext = swr_alloc();
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       audioContext->sample_rate,
                       audioContext->channel_layout, audioContext->sample_fmt,
                       audioContext->sample_rate, 0, 0);
    swr_init(swrContext);
    frame = av_frame_alloc();
    out_buffer = (uint8_t *) av_malloc(44100 * 2);
    *rate = audioContext->sample_rate;
    *channel = audioContext->channels;
    return 0;
}

void getPcm(void **pcm, size_t *pcm_size) {
    int got_frame = -1;
    out_channel_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    while (av_read_frame(formatContext, pPacket) >= 0) {
//            解码  mp3   编码格式frame----pcm   frame
        avcodec_decode_audio4(audioContext, frame, &got_frame, pPacket);
        LOGE("got_frame:%d", got_frame);
        if (got_frame) {
            LOGE("解码");
            swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) frame->data,
                        frame->nb_samples);
//                缓冲区的大小
            int size = av_samples_get_buffer_size(NULL, out_channel_nb, frame->nb_samples,
                                                  AV_SAMPLE_FMT_S16, 1);
            *pcm = out_buffer;
            *pcm_size = (size_t) size;
            break;
        }
    }
    LOGE("getPcm结束");
}

void releaseFFmpeg() {
    av_free(pPacket);
    av_free(out_buffer);
    av_frame_free(&frame);
    swr_free(&swrContext);
    avcodec_close(audioContext);
    avformat_close_input(&formatContext);
}