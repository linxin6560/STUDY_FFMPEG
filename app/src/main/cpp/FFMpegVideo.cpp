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
    //6.一阵一阵读取压缩的视频数据AVPacket
    double last_play  //上一帧的播放时间
    , play             //当前帧的播放时间
    , last_delay    // 上一次播放视频的两帧视频间隔时间
    , delay         //两帧视频间隔时间
    , audio_clock //音频轨道 实际播放时间
    , diff   //音频帧与视频帧相差时间
    , sync_threshold
    , start_time  //从第一帧开始的绝对时间
    , pts
    , actual_delay//真正需要延迟时间
    ;//两帧间隔合理间隔时间
    start_time = av_gettime() / 1000000.0;
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
        if ((pts = av_frame_get_best_effort_timestamp(frame)) == AV_NOPTS_VALUE) {
            pts = 0;
        }
        play = pts * av_q2d(video->time_base);
//        纠正时间
        play = video->synchronize(frame, play);
        delay = play - last_play;
        if (delay <= 0 || delay > 1) {
            delay = last_delay;
        }
        audio_clock = video->audio->clock;
        last_delay = delay;
        last_play = play;
//音频与视频的时间差
        diff = video->clock - audio_clock;
        LOGE("----------------diff:%lf,video->clock:%lf,audio_clock:%lf", diff, video->clock,
             audio_clock);
//        在合理范围外  才会延迟  加快
        sync_threshold = (delay > 0.01 ? 0.01 : delay);

        if (fabs(diff) < 10) {
            if (diff <= -sync_threshold) {
                delay = 0;
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
            }
        }
        start_time += delay;
        actual_delay = start_time - av_gettime() / 1000000.0;
        if (actual_delay < 0.01) {
            actual_delay = 0.01;
        }
        double av_usleep_delay = actual_delay * 1000000.0 + 6000;
        LOGE("last_play:%lf,play:%lf,last_delay:%lf,delay:%lf,audio_clock:%lf,diff:%lf,sync_threshold:%lf,start_time:%lf,pts:%lf,actual_delay:%lf,av_usleep:%lf",
             last_play,
             play,
             last_delay,
             delay,
             audio_clock,
             diff,
             sync_threshold,
             start_time,
             pts,
             actual_delay,
             av_usleep_delay);
        av_usleep(av_usleep_delay);
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
    LOGE("VIDEO stop");

    pthread_mutex_lock(&mutex);
    isPlay = 0;
    //因为可能卡在 deQueue
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    pthread_join(t_playid, 0);
    LOGE("VIDEO join pass");
    if (this->codec) {
        if (avcodec_is_open(this->codec))
            avcodec_close(this->codec);
        avcodec_free_context(&this->codec);
        this->codec = 0;
    }
    LOGE("VIDEO close");
}

void FFMpegVideo::setAVCodecContext(AVCodecContext *avCodecContext) {
    codec = avCodecContext;
}

void FFMpegVideo::setPlayCall(void (*call)(AVFrame *)) {
    video_call = call;
}

double FFMpegVideo::synchronize(AVFrame *frame, double play) {
    //clock是当前播放的时间位置
    if (play != 0)
        clock = play;
    else //pst为0 则先把pts设为上一帧时间
        play = clock;
    //可能有pts为0 则主动增加clock
    //frame->repeat_pict = 当解码时，这张图片需要要延迟多少
    //需要求出扩展延时：
    //extra_delay = repeat_pict / (2*fps) 显示这样图片需要延迟这么久来显示
    double repeat_pict = frame->repeat_pict;
    //使用AvCodecContext的而不是stream的
    double frame_delay = av_q2d(codec->time_base);
    //如果time_base是1,25 把1s分成25份，则fps为25
    //fps = 1/(1/25)
    double fps = 1 / frame_delay;
    //pts 加上 这个延迟 是显示时间
    double extra_delay = repeat_pict / (2 * fps);
    double delay = extra_delay + frame_delay;
//    LOGI("extra_delay:%f",extra_delay);
    clock += delay;
    LOGE("video->clock:%lf", clock);
    return play;
}

void FFMpegVideo::setAudio(FFMpegAudio *audio) {
    this->audio = audio;
}
