#include <jni.h>
#include <string>
#include "my_log.h"
#include "video-player.h"

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
    initContext(&formatContext, input, &codecContext, &codec, &packet);
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
    int result = initContext(&formatContext, input, &videoContext, &codec, &pPacket);
    if (result < 0) {
        LOGE("视频初始化失败:%d", result);
        return;
    }
    //源视频帧
    AVFrame *videoFrame = av_frame_alloc();
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

    // TODO

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