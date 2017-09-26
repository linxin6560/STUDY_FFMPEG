#include <jni.h>
#include "android/log.h"
#include <string>

#define TAG "LEVY"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include <android/native_window_jni.h>
#include <unistd.h>

JNIEXPORT void JNICALL
Java_com_levylin_study_1ffmpeg_MainActivity_transform(JNIEnv *env, jclass type, jstring input_,
                                                      jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);
    LOGE("input=%s", input);
    LOGE("output=%s", output);

    av_register_all();//ffmpeg代码必须调用

    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, input, NULL, NULL) < 0) {
        LOGE("视频打开失败");
        return;
    }
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("找寻流信息失败");
        return;
    }
    //寻找流信息
    int video_stream_index = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        LOGE("循环 %d", i);
        //特别注意，这里的codec_type千万别打成coder_type
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
        }
    }
    LOGE("video_stream_index=%d", video_stream_index);
    if (video_stream_index == -1) {
        LOGE("找寻视频流信息失败");
        return;
    }
    //获取上下文
    AVCodecContext *codecContext = formatContext->streams[video_stream_index]->codec;
    LOGE("获取上下文成功,宽 %d  高 %d", codecContext->width, codecContext->height);
    //获取解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    LOGE("获取解码器成功");
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("解码失败");
        return;
    }
    //分配内存
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(packet);
    LOGE("创建并初始化AVPacket成功");
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
    LOGE("创建和初始化yuvFrame成功");

    FILE *outFile = fopen(output, "wb");
    int got_frame = 0;

    LOGE("开始获取swsContext");
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_YUV420P,
                                            SWS_BILINEAR, NULL, NULL, NULL);
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
    LOGE("input=%s", input);

    av_register_all();//ffmpeg代码必须调用

    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, input, NULL, NULL) < 0) {
        LOGE("视频打开失败");
        return;
    }
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("找寻流信息失败");
        return;
    }
    //寻找流信息
    int video_stream_index = -1;
    int audio_stream_index = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        LOGE("循环 %d", i);
        //特别注意，这里的codec_type千万别打成coder_type
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
        }
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }
    LOGE("video_stream_index=%d", video_stream_index);
    if (video_stream_index == -1) {
        LOGE("找寻视频流信息失败");
        return;
    }
    //获取视频上下文
    AVCodecContext *videoContext = formatContext->streams[video_stream_index]->codec;
    LOGE("获取上下文成功,宽 %d  高 %d", videoContext->width, videoContext->height);
    //获取视频解码器
    AVCodec *codec = avcodec_find_decoder(videoContext->codec_id);
    LOGE("获取视频解码器成功");
    if (avcodec_open2(videoContext, codec, NULL) < 0) {
        LOGE("解码失败");
        return;
    }
    //分配内存
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(packet);
    LOGE("创建并初始化AVPacket成功");
    AVFrame *videoFrame = av_frame_alloc();

    int got_video_frame = 0;

    LOGE("开始获取swsContext");
    SwsContext *swsContext = sws_getContext(videoContext->width, videoContext->height,
                                            videoContext->pix_fmt,
                                            videoContext->width, videoContext->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC, NULL, NULL, NULL);

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_Buffer buffer;

    AVFrame *rgbFrame = av_frame_alloc();
    //yuvFrame中缓存区的初始化
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_RGBA, videoContext->width,
                                        videoContext->height));
    int re = avpicture_fill((AVPicture *) rgbFrame, out_buffer, AV_PIX_FMT_RGBA,
                            videoContext->width,
                            videoContext->height);
    LOGE("开始播放");
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            avcodec_decode_video2(videoContext, videoFrame, &got_video_frame, packet);
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
                    memcpy(dst + i * dstStride, src + i * srcStride, srcStride);
                }
                ANativeWindow_unlockAndPost(nativeWindow);
                usleep(1000 * 16);
            }
        }
        av_free_packet(packet);
    }
    ANativeWindow_release(nativeWindow);
    av_frame_free(&videoFrame);
    av_frame_free(&rgbFrame);
    avcodec_close(videoContext);
    avformat_free_context(formatContext);
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