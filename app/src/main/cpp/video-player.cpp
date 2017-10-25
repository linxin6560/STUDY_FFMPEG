//
// Created by Administrator on 2017/10/15.
//
#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "FFMpegAudio.h"
#include "FFMpegVideo.h"

const char *path;
FFMpegAudio *audio;
FFMpegVideo *video;
pthread_t p_tid;
int isPlay;

ANativeWindow *window;

void call_video_play(AVFrame *frame) {
    if (!window) {
        LOGE("window is null");
        return;
    }
    ANativeWindow_setBuffersGeometry(window, video->codec->width,
                                     video->codec->height, WINDOW_FORMAT_RGBA_8888);
    LOGE("ANativeWindow_setBuffersGeometry width=%d,height=%d", video->codec->width,
         video->codec->height);
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window, &buffer, NULL)) {
        LOGE("window lock fail");
        return;
    }
    //取window的首地址
    uint8_t *dst = (uint8_t *) buffer.bits;
    //一行有多少字节
    int dstStride = buffer.stride * 4;
    //像素数据首地址
    uint8_t *src = frame->data[0];
    //实际一行内存数量
    int srcStride = frame->linesize[0];
    LOGE("TVPlayer...dstStride=%d,srcStride=%d,height=%d", dstStride, srcStride,
         video->codec->height);
    for (int i = 0; i < video->codec->height; ++i) {
        memcpy(dst + i * dstStride, src + i * srcStride, (size_t) srcStride);
    }
    ANativeWindow_unlockAndPost(window);
}

void *proccess(void *args) {
    LOGE("开始播放");
    LOGE("path=%s", path);
    av_register_all();//ffmpeg代码必须调用
    avformat_network_init();//初始化网络组件
    AVFormatContext *formatContext = avformat_alloc_context();
    int result = avformat_open_input(&formatContext, path, NULL, NULL);
    if (result < 0) {
        LOGE("音频打开失败:%d", result);
    }
    result = avformat_find_stream_info(formatContext, NULL);
    if (result < 0) {
        LOGE("找寻流信息失败:%d", result);
    }
    //寻找流信息
    int i = 0;
    for (; i < formatContext->nb_streams; i++) {
        LOGE("循环 %d", i);
        //特别注意，这里的codec_type千万别打成coder_type
        int type = (formatContext)->streams[i]->codec->codec_type;
        //获取上下文
        AVCodecContext *codecContext = formatContext->streams[i]->codec;
        //获取解码器
        AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
        result = avcodec_open2(codecContext, codec, NULL);
        if (result < 0) {
            LOGE("解码失败:%d", result);
            continue;
        }
        LOGE("解码成功");
        if (type == AVMEDIA_TYPE_AUDIO) {
            audio->setAVCodecContext(codecContext);
            audio->time_base = formatContext->streams[i]->time_base;
            audio->index = i;
        } else if (type == AVMEDIA_TYPE_VIDEO) {
            video->setAVCodecContext(codecContext);
            video->time_base = formatContext->streams[i]->time_base;
            video->index = i;
        }
    }
    LOGE("解码结束");

    video->play();
    audio->play();
    isPlay = 1;
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    int ret;
    while (isPlay) {
        ret = av_read_frame(formatContext, packet);
        if (ret >= 0) {
            if (video && video->isPlay && packet->stream_index == video->index) {
                video->put(packet);
            } else if (audio && audio->isPlay && packet->stream_index == audio->index) {
                audio->put(packet);
            }
        }
        av_packet_unref(packet);
    }

    isPlay = 0;//视频解码完，视频可能播放完，也有可能没播放完
    if (video && video->isPlay) {
        video->stop();
    }
    if (audio && audio->isPlay) {
        audio->stop();
    }
    av_free_packet(packet);
    avformat_free_context(formatContext);
    pthread_exit(0);//不加会崩溃
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_play(JNIEnv *env, jobject instance, jstring path_) {
    path = env->GetStringUTFChars(path_, 0);

    audio = new FFMpegAudio;
    video = new FFMpegVideo;

    video->setPlayCall(call_video_play);
    video->setAudio(audio);

    pthread_create(&p_tid, NULL, proccess, NULL);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_stop(JNIEnv *env, jobject instance) {
    if (isPlay) {
        isPlay = 0;
        pthread_join(p_tid, 0);
    }
    if (video) {
        if (video->isPlay) {
            video->stop();
        }
        delete (video);
        video = 0;
    }
    if (audio) {
        if (audio->isPlay) {
            audio->stop();
        }
        delete (audio);
        audio = 0;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_display(JNIEnv *env, jobject instance, jobject surface) {

    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    window = ANativeWindow_fromSurface(env, surface);
    LOGE("ANativeWindow_fromSurface success");
}