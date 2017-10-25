//
// Created by Administrator on 2017/10/15.
//

#include "FFMpegAudio.h"


FFMpegAudio::FFMpegAudio() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    clock = 0;
}

FFMpegAudio::~FFMpegAudio() {

}

int createFFmpeg(FFMpegAudio *audio) {
    audio->swrContext = swr_alloc();
    swr_alloc_set_opts(audio->swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       audio->codec->sample_rate,
                       audio->codec->channel_layout, audio->codec->sample_fmt,
                       audio->codec->sample_rate, 0, 0);
    swr_init(audio->swrContext);
    audio->out_buffer = (uint8_t *) av_malloc(44100 * 2);
    LOGE("初始化FFMPEG成功");
    return 0;
}

int getPCM(FFMpegAudio *audio) {
    int got_frame = -1;
    AVFrame *frame = av_frame_alloc();
    AVPacket *pPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
    int out_channel_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    int out_buffer_size = 0;
    LOGE("getPCM...audio->isPlay=%d", audio->isPlay);
    while (audio->isPlay) {
//            解码  mp3   编码格式frame----pcm   frame
        audio->get(pPacket);
        LOGE("获取packet成功");
        if (pPacket->pts != AV_NOPTS_VALUE) {
            audio->clock = pPacket->pts * av_q2d(audio->time_base);
            LOGE("audio->clock=%lf", audio->clock);
        }
        avcodec_decode_audio4(audio->codec, frame, &got_frame, pPacket);
        if (got_frame) {
            LOGE("解码");
            swr_convert(audio->swrContext, &audio->out_buffer, 44100 * 2,
                        (const uint8_t **) frame->data,
                        frame->nb_samples);
//                缓冲区的大小
            out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb, frame->nb_samples,
                                                         AV_SAMPLE_FMT_S16, 1);
            break;
        }
    }
    av_free(pPacket);
    av_frame_free(&frame);
    LOGE("getPcm结束");
    return out_buffer_size;
}


/**
 * 喇叭一读完，就会回调这个函数,添加pcm到缓冲区
 * @param bq
 * @param context
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    LOGE("=================bqPlayerCallback====================");
    FFMpegAudio *audio = (FFMpegAudio *) context;
    int size = getPCM(audio);
    LOGE("getPCM.size=%d", size);
    double time = size / ((double)44100 * 2 * 2);
    audio->clock += time;
    LOGE("当前一帧声音时间%f   播放时间%f", time, audio->clock);
    if (size > 0) {
        int result = (*bq)->Enqueue(bq, audio->out_buffer, (SLuint32) size);
        LOGE("播放result=%d", result);
    }
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
    audio->createPlayer();
    pthread_exit(0);
}

void FFMpegAudio::play() {
    isPlay = 1;
    pthread_create(&t_playid, NULL, play_audio, this);
}

void FFMpegAudio::stop() {
    LOGE("声音暂停");
    //因为可能卡在 deQueue
    pthread_mutex_lock(&mutex);
    isPlay = 0;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    pthread_join(t_playid, 0);
    if (bqPlayerItf) {
        (*bqPlayerItf)->SetPlayState(bqPlayerItf, SL_PLAYSTATE_STOPPED);
        bqPlayerItf = 0;
    }
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;

        bqPlayQueue = 0;
        volumeItf = 0;
    }

    if (outputMix) {
        (*outputMix)->Destroy(outputMix);
        outputMix = 0;
    }

    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineObject = 0;
    }
    if (swrContext)
        swr_free(&swrContext);
    if (this->codec) {
        if (avcodec_is_open(this->codec))
            avcodec_close(this->codec);
        avcodec_free_context(&this->codec);
        this->codec = 0;
    }
    LOGE("AUDIO clear");
}

void FFMpegAudio::setAVCodecContext(AVCodecContext *avCodecContext) {
    codec = avCodecContext;
    //初始化FFmpeg
    createFFmpeg(this);
}

int FFMpegAudio::createPlayer() {
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
    (*bqPlayQueue)->RegisterCallback(bqPlayQueue, bqPlayerCallback, this);
    LOGE("设置播放状态");
    (*bqPlayerItf)->SetPlayState(bqPlayerItf, SL_PLAYSTATE_PLAYING);
    LOGE("获取音量对象");
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &volumeItf);
    LOGE("播放第一帧");
    bqPlayerCallback(bqPlayQueue, this);
    return 1;
}