//
// Created by Administrator on 2017/9/26.
//

#include "player.h"
#include "my_log.h"

int initVideoContext(AVFormatContext **formatContext, const char *input,
                     AVCodecContext **pCodecContext,
                     AVCodec **pAVCodec, AVPacket **pPacket,enum AVMediaType type) {
    LOGE("input=%s", input);
    *formatContext = avformat_alloc_context();
    int result = avformat_open_input(formatContext, input, NULL, NULL);
    if (result < 0) {
        LOGE("视频打开失败:%d", result);
        return result;
    }
    result = avformat_find_stream_info(*formatContext, NULL);
    if (result < 0) {
        LOGE("找寻流信息失败:%d", result);
        return result;
    }
    //寻找流信息
    int video_stream_index = -1;
    for (int i = 0; i < (*formatContext)->nb_streams; ++i) {
        LOGE("循环 %d", i);
        //特别注意，这里的codec_type千万别打成coder_type
        if ((*formatContext)->streams[i]->codec->codec_type == type) {
            video_stream_index = i;
        }
    }
    LOGE("video_stream_index=%d", video_stream_index);
    if (video_stream_index == -1) {
        LOGE("找寻视频流信息失败");
        return -1;
    }
    //获取上下文
    *pCodecContext = (*formatContext)->streams[video_stream_index]->codec;
    LOGE("获取上下文成功,宽 %d  高 %d", (*pCodecContext)->width, (*pCodecContext)->height);
    //获取解码器
    *pAVCodec = avcodec_find_decoder((*pCodecContext)->codec_id);
    LOGE("获取解码器成功");
    result = avcodec_open2((*pCodecContext), *pAVCodec, NULL);
    if (result < 0) {
        LOGE("解码失败:%d", result);
        return result;
    }
    *pPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(*pPacket);
    LOGE("创建并初始化AVPacket成功");
    return 1;
}