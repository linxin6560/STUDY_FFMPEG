//
// Created by Administrator on 2017/10/15.
//

#ifndef STUDY_FFMPEG_FFMPEGVIDEO_H
#define STUDY_FFMPEG_FFMPEGVIDEO_H

#include <pthread.h>
#include "my_log.h"
#include "FFMpegAudio.h"
#include <queue>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

class FFMpegVideo {

public:
    FFMpegVideo();

    ~FFMpegVideo();

    int get(AVPacket *packet);

    int put(AVPacket *packet);

    void play();

    void stop();

    void setAVCodecContext(AVCodecContext *avCodecContext);

    void setPlayCall(void(*call)(AVFrame *frame));

    double synchronize(AVFrame *frame, double play);

    void setAudio(FFMpegAudio *audio);

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

    AVRational time_base;
    //头文件写在"extend"C外
    FFMpegAudio* audio;
    double  clock;
};


#endif //STUDY_FFMPEG_FFMPEGVIDEO_H
