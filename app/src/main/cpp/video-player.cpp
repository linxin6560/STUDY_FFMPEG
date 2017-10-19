//
// Created by Administrator on 2017/10/15.
//
#include <jni.h>
#include <string>
#include <unistd.h>
#include "FFMpegAudio.h"
#include "FFMpegVideo.h"

const char *path;
FFMpegAudio *audio;
FFMpegVideo *video;
pthread_t p_tid;
int isPlay;

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
            audio->index = i;
        } else if (type == AVMEDIA_TYPE_VIDEO) {
            video->setAVCodecContext(codecContext);
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
//                video->put(packet);
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
Java_com_levylin_study_1ffmpeg_TVPlayer_player(JNIEnv *env, jobject instance) {

// TODO

}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_play(JNIEnv *env, jobject instance, jstring path_) {
    path = env->GetStringUTFChars(path_, 0);

    audio = new FFMpegAudio;
    video = new FFMpegVideo;


    pthread_create(&p_tid, NULL, proccess, NULL);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_stop(JNIEnv *env, jobject instance) {

// TODO

}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_release(JNIEnv *env, jobject instance) {

// TODO

}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_TVPlayer_display(JNIEnv *env, jobject instance, jobject holder) {

// TODO

}