//
// Created by Administrator on 2017/10/15.
//
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "FFMpegMusic.h"

SLObjectItf engineObject;
SLEngineItf slEngineItf;
SLObjectItf outputMix;
SLEnvironmentalReverbItf environment;
SLEnvironmentalReverbSettings settings;
SLObjectItf bqPlayerObject;
SLPlayItf bqPlayerItf;
SLAndroidSimpleBufferQueueItf bqPlayQueue;//缓冲区
SLVolumeItf volumeItf;//音量对象
void *buffer;
size_t bufferSize = 0;

/**
 * 喇叭一读完，就会回调这个函数,添加pcm到缓冲区
 * @param queueItf
 * @param context
 */
void bqPlayCallback(SLAndroidSimpleBufferQueueItf queueItf, void *context) {
    bufferSize = 0;
    getPcm(&buffer, &bufferSize);
    LOGE("bqPlayCallback:%d", bufferSize);
    if (buffer != NULL && bufferSize != 0) {
        int result = (*bqPlayQueue)->Enqueue(bqPlayQueue, buffer, bufferSize);
        LOGE("播放result=%d", result);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_OpenSLESPlayer_shutdown(JNIEnv *env, jobject instance) {
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (bqPlayerObject != NULL) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerItf = NULL;
        bqPlayQueue = NULL;
        volumeItf = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMix != NULL) {
        (*outputMix)->Destroy(outputMix);
        outputMix = NULL;
        environment = NULL;
    }
    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        slEngineItf = NULL;
    }
    // 释放FFmpeg解码器相关资源
    releaseFFmpeg();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_OpenSLESPlayer_play(JNIEnv *env, jobject instance, jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    LOGE("初始化引擎");
    slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);

    LOGE("获取引擎接口");
    (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &slEngineItf);

    LOGE("获取混音器");
    (*slEngineItf)->CreateOutputMix(slEngineItf, &outputMix, 0, 0, 0);
    (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);

    LOGE("设置环境混响");
    int result = (*outputMix)->GetInterface(outputMix, SL_IID_ENVIRONMENTALREVERB, &environment);

    if (result == SL_RESULT_SUCCESS) {
        (*environment)->SetEnvironmentalReverbProperties(environment, &settings);
    }

    //初始化FFmpeg
    int rate;
    int channels;
    createFFmpeg(path, &rate, &channels);
    LOGE(" 比特率%d  ,channels %d", rate, channels);

    LOGE("创建SLDataSource");
    SLDataLocator_AndroidBufferQueue queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = 2;
    format_pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
    format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    SLDataSource dataSource = {&queue, &format_pcm};

    LOGE("配置SLDataSink");
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMix};
    SLDataSink audioSnk = {&loc_outmix, NULL};
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    LOGE("创建播放器");
    (*slEngineItf)->CreateAudioPlayer(slEngineItf, &bqPlayerObject, &dataSource, &audioSnk, 3, ids,
                                      req);

    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    LOGE("创建播放器接口");
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerItf);
    LOGE("创建队列缓冲区");
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayQueue);
    LOGE("注册回调接口");
    (*bqPlayQueue)->RegisterCallback(bqPlayQueue, bqPlayCallback, NULL);
    LOGE("设置播放状态");
    (*bqPlayerItf)->SetPlayState(bqPlayerItf, SL_PLAYSTATE_PLAYING);
    LOGE("获取音量对象");
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &volumeItf);
    LOGE("播放第一帧");
    bqPlayCallback(bqPlayQueue, NULL);
    env->ReleaseStringUTFChars(path_, path);
}

