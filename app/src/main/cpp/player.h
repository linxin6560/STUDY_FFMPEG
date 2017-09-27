//
// Created by Administrator on 2017/9/26.
//

#ifndef STUDY_FFMPEG_VIDEO_PLAYER_H
#define STUDY_FFMPEG_VIDEO_PLAYER_H


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include <android/native_window_jni.h>
#include <unistd.h>
};

int initVideoContext(AVFormatContext **formatContext, const char *input,
                     AVCodecContext **pCodecContext,
                     AVCodec **pAVCodec, AVPacket **pPacket, enum AVMediaType type);

#endif //STUDY_FFMPEG_VIDEO_PLAYER_H