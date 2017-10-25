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

static void (*video_call)(AVFrame *frame);

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
    FFMpegVideo *video = (FFMpegVideo *) args;
    //像素数据（解码数据）
    AVFrame *frame = av_frame_alloc();

    //转换rgba
    SwsContext *swsContext = sws_getContext(video->codec->width,
                                            video->codec->height,
                                            video->codec->pix_fmt,
                                            video->codec->width,
                                            video->codec->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC, NULL, NULL, NULL);

    AVFrame *rgb_frame = av_frame_alloc();
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_RGBA, video->codec->width,
                                        video->codec->height));
    avpicture_fill((AVPicture *) rgb_frame, out_buffer, AV_PIX_FMT_RGBA,
                   video->codec->width,
                   video->codec->height);
    int got_frame;
    LOGE("宽  %d ,高  %d ", video->codec->width, video->codec->height);
    //编码数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    while (video->isPlay) {
        LOGE("视频 解码  一帧 %d", video->queue.size());
//        消费者取到一帧数据  没有 阻塞
        video->get(packet);
        avcodec_decode_video2(video->codec, frame, &got_frame, packet);
        if (!got_frame) {
            continue;
        }
//        转码成rgb
        sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0,
                                     frame->height,
                                     rgb_frame->data, rgb_frame->linesize);
        av_usleep(16 * 1000);
        video_call(rgb_frame);
        av_packet_unref(packet);
    }
    LOGE("free packet");
    av_free(packet);
    LOGE("free packet ok");
    LOGE("free packet");
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    sws_freeContext(swsContext);
    size_t size = video->queue.size();
    for (int i = 0; i < size; ++i) {
        AVPacket *pkt = video->queue.front();
        av_free(pkt);
        video->queue.pop();
    }
    LOGE("VIDEO EXIT");
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

void FFMpegVideo::setPlayCall(void (*call)(AVFrame *)) {
    video_call = call;
}
