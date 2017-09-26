package com.levylin.study_ffmpeg;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("avcodec-56");
        System.loadLibrary("avdevice-56");
        System.loadLibrary("avfilter-5");
        System.loadLibrary("avformat-56");
        System.loadLibrary("avutil-54");
        System.loadLibrary("postproc-53");
        System.loadLibrary("swresample-1");
        System.loadLibrary("swscale-3");
    }

    private VideoView videoView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        videoView = (VideoView) findViewById(R.id.videoView);
    }


    public void click2transform(View view) {
        String inputPath = Environment.getExternalStorageDirectory().getPath() + "/input.mp4";
        String outPath = Environment.getExternalStorageDirectory().getPath() + "/output.yuv";
        transform(inputPath, outPath);
    }

    public static native void transform(String input, String output);

    public void click2play(View view) {
        String inputPath = Environment.getExternalStorageDirectory().getPath() + "/input.mp4";
        videoView.play(inputPath);
    }
}
