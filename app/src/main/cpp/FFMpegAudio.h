//
// Created by Administrator on 2017/10/15.
//

#ifndef STUDY_FFMPEG_FFMPEGAUDIO_H
#define STUDY_FFMPEG_FFMPEGAUDIO_H

#include "my_log.h"
#include <pthread.h>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
}

class FFMpegAudio {

public:
    FFMpegAudio();

    ~FFMpegAudio();

    int get(AVPacket *packet);

    int put(AVPacket *packet);

    void play();

    void stop();

    void setAVCodecContext(AVCodecContext *avCodecContext);

public:
    //是否播放
    int isPlay;
    //流索引
    int index;
    //音频队列
    std::queue<AVPacket *> queue;
    //处理线程
    pthread_t t_playid;
    //解码器上下文
    AVCodecContext *codec;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

};

#endif //STUDY_FFMPEG_FFMPEGAUDIO_H