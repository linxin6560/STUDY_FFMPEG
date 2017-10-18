//
// Created by Administrator on 2017/10/15.
//

#include "FFMpegAudio.h"


FFMpegAudio::FFMpegAudio() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

FFMpegAudio::~FFMpegAudio() {

}

int FFMpegAudio::get(AVPacket *packet) {
    pthread_mutex_lock(&mutex);
    while (isPlay) {
        bool empty = queue.empty();
        if (!empty) {
            //从队列去取一个packet，克隆到对象
            if (av_packet_ref(packet, queue.front()) < 0) {
                LOGE("克隆队列音频packet失败");
                break;
            }
            LOGE("克隆队列音频packet成功");
            AVPacket *packet1 = queue.front();
            queue.pop();
            av_free(packet1);
            break;
        } else {
            LOGE("音频队列为空");
            pthread_cond_wait(&cond, &mutex);
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

int FFMpegAudio::put(AVPacket *packet) {
    AVPacket *packet1 = (AVPacket *) av_malloc(sizeof(AVPacket));
    if (av_copy_packet(packet1, packet) < 0) {
        LOGE("克隆失败");
        return 0;

    }
    LOGE("压入一帧音频数据");
    pthread_mutex_lock(&mutex);
    queue.push(packet1);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return 0;
}

void *play_audio(void *args) {
    LOGE("开启音频线程");
    FFMpegAudio *audio = (FFMpegAudio *) args;
    AVPacket *pPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    //初始化结构体
    av_init_packet(pPacket);
    LOGE("创建并初始化AVPacket成功");
    SwrContext *swrContext = swr_alloc();
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       audio->codec->sample_rate,
                       audio->codec->channel_layout, audio->codec->sample_fmt,
                       audio->codec->sample_rate, 0, 0);
    swr_init(swrContext);
    AVFrame *frame = av_frame_alloc();
    uint8_t *out_buffer = (uint8_t *) av_malloc(44100 * 2 * 2);
    int channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    int got_frame = -1;
    while (audio->isPlay) {
        audio->get(pPacket);
        LOGE("消费一帧音频数据,queue.size=%d",audio->queue.size());
        avcodec_decode_audio4(audio->codec, frame, &got_frame, pPacket);
        if (got_frame) {
            swr_convert(swrContext, &out_buffer, 44100 * 2 * 2, (const uint8_t **) frame->data,
                        frame->nb_samples);
        }
        int av_buffer_size = av_samples_get_buffer_size(NULL, channels, frame->nb_samples,
                                                        AV_SAMPLE_FMT_S16, 1);

    }
}

void FFMpegAudio::play() {
    isPlay = 1;
    pthread_create(&t_playid, NULL, play_audio, this);
}

void FFMpegAudio::stop() {

}

void FFMpegAudio::setAVCodecContext(AVCodecContext *avCodecContext) {
    codec = avCodecContext;
}
