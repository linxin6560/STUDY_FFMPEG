package com.levylin.study_ffmpeg;

import android.view.SurfaceHolder;

/**
 * 视频播放器
 * Created by LinXin on 2017/10/15.
 */
public class TVPlayer {

    public native void player();

    public native void play(String path);

    public native void stop();

    public native void release();

    public native void display(SurfaceHolder holder);
}
