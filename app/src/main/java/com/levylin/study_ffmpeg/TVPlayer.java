package com.levylin.study_ffmpeg;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * 视频播放器
 * Created by LinXin on 2017/10/15.
 */
public class TVPlayer extends SurfaceView implements SurfaceHolder.Callback {

    public TVPlayer(Context context) {
        super(context);
        init();
    }

    public TVPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public TVPlayer(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        SurfaceHolder holder = getHolder();
        holder.setFormat(PixelFormat.RGB_888);
        holder.addCallback(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        display(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {
        display(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    public native void play(String path);

    public native void stop();

    public native void display(Surface surface);
}
