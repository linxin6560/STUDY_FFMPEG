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
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class FFMpegAudio {

public:
    FFMpegAudio();

    ~FFMpegAudio();

    int get(AVPacket *packet);

    int put(AVPacket *packet);

    void play();

    void stop();

    void setAVCodecContext(AVCodecContext *avCodecContext);

    int createPlayer();

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

    SwrContext *swrContext;
    uint8_t *out_buffer;

    double clock;//第一帧音频帧走过的时间，实时更新
    AVRational time_base;//

    SLObjectItf engineObject;
    SLEngineItf slEngineItf;
    SLObjectItf outputMix;
    SLEnvironmentalReverbItf environment;
    SLEnvironmentalReverbSettings settings;
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerItf;
    SLAndroidSimpleBufferQueueItf bqPlayQueue;//缓冲区
    SLVolumeItf volumeItf;//音量对象
};
};
#endif //STUDY_FFMPEG_FFMPEGAUDIO_H
