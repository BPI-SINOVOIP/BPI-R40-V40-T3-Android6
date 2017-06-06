/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;

import android.os.Binder;
import android.os.IDynamicPManager;
import android.os.DynamicPManager;
import android.os.SystemClock;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.SystemProperties;
import android.util.Log;
import android.util.Slog;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Iterator;
import java.util.Timer;
import java.util.TimerTask;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import android.media.MediaPlayer;
import android.media.AudioManager;

import com.android.server.NativeDaemonConnector.Command;
import com.android.server.NativeDaemonConnector.SensitiveArg;
import com.android.server.am.ActivityManagerService;
import com.android.server.pm.PackageManagerService;
import com.android.server.pm.UserManagerService;

public class DynamicPManagerService extends IDynamicPManager.Stub
        implements INativeDaemonConnectorCallbacks {
    private static final String TAG = "DynamicPManagerService";
    private static final String SAYEYE_TAG = "SayeyeConnector";
    private static final boolean LOCAL_LOGD = false;

    private static final String LAUNCHER = "com.android.launcher";
    private int mScense = ScenseState.NORMAL;
    private volatile boolean mSystemReady = false;
    private volatile boolean mStatus = false;
    private volatile boolean mScreen = false;

    /** Maximum number of ASEC containers allowed to be mounted. */
    private static final int MAX_CONTAINERS = 250;


    private final String mSync = "sync";

    private int VIDEO_PLAYING = 1;
    private int DECODE_HW = 1;
    private int mCurLayers = 0;

    private Timer mTimer = null;

    private AudioManager mAudioService;


    class ScenseCallBack {
        String pkg_name;
        String aty_name;
        int pid;
        int tags;

        ScenseCallBack(String pkg_name, String aty_name, int pid, int tags) {
            this.pkg_name = pkg_name;
            this.aty_name = aty_name;
            this.pid = pid;
            this.tags = tags;
        }

        void handleFinished() {
            if (LOCAL_LOGD) Slog.d(TAG, "ScenseSetting ");
            mScense = 0;
        }
    }

    /*
     * Internal sayeye volume state constants
     */
    class ScenseState {
        public static final int NORMAL          = 0x00000001;
        public static final int HOME            = 0x00000002;
        public static final int BOOTCOMPLETE    = 0x00000004;
        public static final int VIDEO           = 0x00000008;
        public static final int MUSIC           = 0x00000010;
        public static final int MONITOR         = 0x00000020;
        public static final int ROTATE          = 0x00000040;
        public static final int BENCHMARK       = 0x00000080;
    }

    /*
     * Internal scense response code constants
     */
    class ScenseResponseCode {
        static final int ScenseNormalResult         = 610;
        static final int ScenseHomeResult           = 611;
        static final int ScenseBootCompleteResult   = 612;
        static final int ScenseVideoResult          = 613;
        static final int ScenseMusicResult          = 614;
        static final int ScenseMonitorResult        = 615;
        static final int ScenseRotateResult         = 616;
        static final int ScenseBenchmarkResult      = 617;

    };

    private String ToString(int scense) {
        switch (scense) {
            case ScenseState.NORMAL:
                return "normal";
            case ScenseState.HOME:
                return "home";
            case ScenseState.BOOTCOMPLETE:
                return "bootcomplete";
            case ScenseState.VIDEO:
                return "video";
            case ScenseState.MUSIC:
                return "music";
            case ScenseState.MONITOR:
                return "monitor";
            case ScenseState.ROTATE:
                return "rotate";
            case ScenseState.BENCHMARK:
                return "benchmark";
            default:
                break;
        }

        return "un-defined scense";
    }

    class DynamicPManagerServiceHandler extends Handler {
        boolean mUpdatingStatus = false;

        DynamicPManagerServiceHandler(Looper l) {
            super(l);
        }
    };

    /**
     * Callback from NativeDaemonConnector
     */

    public void onDaemonConnected() {
        /*
         * Since we'll be calling back into the NativeDaemonConnector,
         * we need to do our work in a new thread.
         */
        new Thread("DynamicPManagerService#onDaemonConnected") {
            @Override
            public void run() {
                Slog.i(TAG, "sayeye has connected");
            }
        }.start();
    }
    /**
     * Callback from NativeDaemonConnector
     */
    public boolean onCheckHoldWakeLock(int code) {
        return false;
    }

    /**
     * Callback from NativeDaemonConnector
     */
    public boolean onEvent(int code, String raw, String[] cooked) {
               return true;
    }

    public void systemReady() {
    }
    
    /**
     * Boost receiver
     */
    private final BroadcastReceiver mBoostReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Slog.i(TAG, "Receiver notifyDPM");
          }

    };

    public DynamicPManagerService(Context context) {
         /*
         * Create the connection to vold with a maximum queue of twice the
         * amount of containers we'd ever expect to have. This keeps an
         * "asec list" from blocking a thread repeatedly.
         */
       
    }

    public void notifyDPM(Intent intent){
        
    }
}
