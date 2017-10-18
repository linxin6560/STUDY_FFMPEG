//
// Created by Administrator on 2017/10/15.
//

#include "FFMpegVideo.h"

FFMpegVideo::FFMpegVideo() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

FFMpegVideo::~FFMpegVideo() {

}

int FFMpegVideo::get(AVPacket *packet) {
    pthread_mutex_lock(&mutex);
    while (isPlay) {
        if (!queue.empty()) {
            //从队列去取一个packet，克隆到对象
            if (av_packet_ref(packet, queue.front()) < 0) {
                LOGE("克隆队列视频packet失败");
                break;
            }
            LOGE("克隆队列视频packet成功");
            AVPacket *packet1 = queue.front();
            queue.pop();
            av_free(packet1);
            break;
        } else {
            LOGE("视频队列为空");
            pthread_cond_wait(&cond, &mutex);
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

int FFMpegVideo::put(AVPacket *packet) {
    AVPacket *packet1 = (AVPacket *) av_malloc(sizeof(AVPacket));
    if (av_copy_packet(packet1, packet) < 0) {//不能用av_packet_ref,存进去要重新复制一份，ref只是引用
        LOGE("克隆失败");
        return 0;

    }
    LOGE("压入一帧视频数据");
    pthread_mutex_lock(&mutex);
    queue.push(packet1);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return 0;
}

void *play_video(void *args) {
    LOGE("开启视频线程");
    FFMpegVideo *video = (FFMpegVideo *) args;
    AVPacket *pPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(pPacket);
    LOGE("创建并初始化AVPacket成功");
    AVFrame *frame = av_frame_alloc();
    //声明一个yuvFrame
    AVFrame *rgb_frame = av_frame_alloc();
    //yuvFrame中缓存区的初始化
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_YUV420P,
                                        video->codec->width,
                                        video->codec->height));
    avpicture_fill((AVPicture *) rgb_frame, out_buffer, AV_PIX_FMT_YUV420P,
                   video->codec->width,
                   video->codec->height);
    LOGE("开始获取swsContext");
    SwsContext *swsContext = sws_getContext(video->codec->width, video->codec->height,
                                            video->codec->pix_fmt,
                                            video->codec->width, video->codec->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC, NULL, NULL, NULL);
    int got_frame = -1;
    while (video->isPlay) {
        video->get(pPacket);
        LOGE("消费一帧视频数据,queue.size=%d",video->queue.size());
        avcodec_decode_video2(video->codec, frame, &got_frame, pPacket);
        if (got_frame) {
            sws_scale(swsContext, (const uint8_t *const *) frame->data,
                      frame->linesize, 0,
                      frame->height, rgb_frame->data, rgb_frame->linesize);
        }
    }
    LOGE("视频播放结束");
    pthread_exit(0);
}

void FFMpegVideo::play() {
    isPlay = 1;
    pthread_create(&t_playid, NULL, play_video, this);
}

void FFMpegVideo::stop() {

}

void FFMpegVideo::setAVCodecContext(AVCodecContext *avCodecContext) {
    codec = avCodecContext;
}
