package com.levylin.study_ffmpeg;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

/**
 * 电视台activity
 * Created by LinXin on 2017/10/17.
 */
public class TVActivity extends AppCompatActivity {

    TVPlayer player;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_tv);
        player = (TVPlayer) findViewById(R.id.surfaceView);
    }

    public void play(View view) {
        player.play("rtmp://live.hkstv.hk.lxdns.com/live/hks");
    }

    public void stop(View view) {
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.stop();
    }
}
