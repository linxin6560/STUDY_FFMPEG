#include <jni.h>
#include <string>
#include "my_log.h"
#include "player.h"
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

extern "C" {
JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_MainActivity_transform(JNIEnv *env, jclass type, jstring input_,
                                                      jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);
    LOGE("input=%s", input);
    LOGE("output=%s", output);
    av_register_all();//ffmpeg代码必须调用
    AVFormatContext *formatContext;
    //获取上下文
    AVCodecContext *codecContext;
    //获取解码器
    AVCodec *codec;
    AVPacket *packet;
    initVideoContext(&formatContext, input, &codecContext, &codec, &packet, AVMEDIA_TYPE_VIDEO);
    //分配内存
    LOGE("创建和初始化yuvFrame成功");
    FILE *outFile = fopen(output, "wb");
    int got_frame = 0;
    LOGE("开始获取swsContext");
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_YUV420P,
                                            SWS_BILINEAR, NULL, NULL, NULL);
    AVFrame *frame = av_frame_alloc();
    //声明一个yuvFrame
    AVFrame *yuvFrame = av_frame_alloc();
    //yuvFrame中缓存区的初始化
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_YUV420P, codecContext->width,
                                        codecContext->height));
    int re = avpicture_fill((AVPicture *) yuvFrame, out_buffer, AV_PIX_FMT_YUV420P,
                            codecContext->width,
                            codecContext->height);
    LOGE("开始解码");
    while (av_read_frame(formatContext, packet) >= 0) {
        avcodec_decode_video2(codecContext, frame, &got_frame, packet);
        if (got_frame > 0) {
            sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0,
                      frame->height, yuvFrame->data, yuvFrame->linesize);
            int y_size = codecContext->width * codecContext->height;
            //        y 亮度信息写完了
            fwrite(yuvFrame->data[0], 1, (size_t) y_size, outFile);
            fwrite(yuvFrame->data[1], 1, (size_t) (y_size / 4), outFile);
            fwrite(yuvFrame->data[2], 1, (size_t) (y_size / 4), outFile);
        }
        av_free_packet(packet);
    }
    LOGE("转码成功");
    fclose(outFile);
    av_frame_free(&frame);
    av_frame_free(&yuvFrame);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
}

JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_VideoView_render(JNIEnv *env, jobject instance, jstring input_,
                                                jobject surface) {
    const char *input = env->GetStringUTFChars(input_, 0);
    av_register_all();//ffmpeg代码必须调用
    AVFormatContext *formatContext;
    //获取上下文
    AVCodecContext *videoContext;
    //获取解码器
    AVCodec *codec;
    AVPacket *pPacket;
    int result = initVideoContext(&formatContext, input, &videoContext, &codec, &pPacket,
                                  AVMEDIA_TYPE_VIDEO);
    if (result < 0) {
        LOGE("视频初始化失败:%d", result);
        return;
    }
    LOGE("开始获取swsContext");
    SwsContext *swsContext = sws_getContext(videoContext->width, videoContext->height,
                                            videoContext->pix_fmt,
                                            videoContext->width, videoContext->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC, NULL, NULL, NULL);

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_Buffer buffer;

    //播放的帧
    AVFrame *rgbFrame = av_frame_alloc();
    //rgbFrame中缓存区的初始化
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_RGBA, videoContext->width,
                                        videoContext->height));
    avpicture_fill((AVPicture *) rgbFrame, out_buffer, AV_PIX_FMT_RGBA,
                   videoContext->width,
                   videoContext->height);
    LOGE("开始播放");
    //源视频帧
    AVFrame *videoFrame = av_frame_alloc();
    int got_video_frame = 0;
    while (av_read_frame(formatContext, pPacket) >= 0) {
        avcodec_decode_video2(videoContext, videoFrame, &got_video_frame, pPacket);
        LOGE("播放视频:%d", got_video_frame);
        if (got_video_frame > 0) {
            sws_scale(swsContext, (const uint8_t *const *) videoFrame->data,
                      videoFrame->linesize, 0,
                      videoFrame->height, rgbFrame->data, rgbFrame->linesize);
            //绘制之前配置一些信息：宽高，格式
            ANativeWindow_setBuffersGeometry(nativeWindow, videoContext->width,
                                             videoContext->height, WINDOW_FORMAT_RGBA_8888);
            ANativeWindow_lock(nativeWindow, &buffer, NULL);
            //取window的首地址
            uint8_t *dst = (uint8_t *) buffer.bits;
            //一行有多少字节
            int dstStride = buffer.stride * 4;
            //像素数据首地址
            uint8_t *src = rgbFrame->data[0];
            //实际一行内存数量
            int srcStride = rgbFrame->linesize[0];
            for (int i = 0; i < videoContext->height; ++i) {
                memcpy(dst + i * dstStride, src + i * srcStride, (size_t) srcStride);
            }
            ANativeWindow_unlockAndPost(nativeWindow);
            usleep(1000 * 16);
        }
        av_free_packet(pPacket);
    }
    ANativeWindow_release(nativeWindow);
    av_frame_free(&videoFrame);
    av_frame_free(&rgbFrame);
    avcodec_close(videoContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(input_, input);
}

JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_MusicPlayer_sound(JNIEnv *env, jobject instance, jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    av_register_all();//ffmpeg代码必须调用
    AVFormatContext *formatContext;
    //获取上下文
    AVCodecContext *audioContext;
    //获取解码器
    AVCodec *codec;
    AVPacket *pPacket;
    int result = initVideoContext(&formatContext, input, &audioContext, &codec, &pPacket,
                                  AVMEDIA_TYPE_AUDIO);
    if (result < 0) {
        LOGE("初始化音频上下文失败");
    }
    //初始化swr context
    SwrContext *swrContext = swr_alloc();
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    int out_sample_rate = audioContext->sample_rate;
    swr_alloc_set_opts(swrContext, out_ch_layout, out_format, out_sample_rate,
                       audioContext->channel_layout, audioContext->sample_fmt,
                       audioContext->sample_rate, 0,
                       NULL);
    swr_init(swrContext);

    //读音频
    AVFrame *originFrame = av_frame_alloc();
    int got_frame = -1;
    uint8_t *out_buffer = (uint8_t *) av_malloc(44100 * 2);
    //    获取通道数  2
    int out_channer_nb = av_get_channel_layout_nb_channels(out_ch_layout);
//    反射得到Class类型
    jclass david_player = env->GetObjectClass(instance);
//    反射得到createAudio方法
    jmethodID createAudio = env->GetMethodID(david_player, "createAudio", "(II)V");
//    反射调用createAudio
    env->CallVoidMethod(instance, createAudio, 44100, out_channer_nb);
    jmethodID audio_write = env->GetMethodID(david_player, "playTrack", "([BI)V");

    while (av_read_frame(formatContext, pPacket) >= 0) {
//            解码  mp3   编码格式frame----pcm   frame
        avcodec_decode_audio4(audioContext, originFrame, &got_frame, pPacket);
        if (got_frame) {
            LOGE("解码");
            /**
             * int swr_convert(struct SwrContext *s, uint8_t **out, int out_count,
                            const uint8_t **in , int in_count);
             */
            swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) originFrame->data,
                        originFrame->nb_samples);
//                缓冲区的大小
            int size = av_samples_get_buffer_size(NULL, out_channer_nb, originFrame->nb_samples,
                                                  AV_SAMPLE_FMT_S16, 1);
            jbyteArray audio_sample_array = env->NewByteArray(size);
            env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) out_buffer);
            env->CallVoidMethod(instance, audio_write, audio_sample_array, size);
            env->DeleteLocalRef(audio_sample_array);
        }
    }
    av_frame_free(&originFrame);
    swr_free(&swrContext);
    avcodec_close(audioContext);
    avformat_close_input(&formatContext);
    env->ReleaseStringUTFChars(input_, input);
}

JNIEXPORT jstring JNICALL
Java_com_levylin_study_1ffmpeg_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
}