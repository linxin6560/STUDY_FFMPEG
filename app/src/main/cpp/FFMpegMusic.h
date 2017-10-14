//
// Created by Administrator on 2017/10/12.
//

#ifndef STUDY_FFMPEG_FFMPEGMUSIC_H
#define STUDY_FFMPEG_FFMPEGMUSIC_H

#endif //STUDY_FFMPEG_FFMPEGMUSIC_H

extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
//像素处理
#include "libswscale/swscale.h"
#include <android/native_window_jni.h>
#include <unistd.h>
#include "my_log.h"
}

int createFFmpeg(const char *path, int *rate, int* channel);

void getPcm(void **pcm, size_t *size);

void releaseFFmpeg();

