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

import android.app.ActivityManagerNative;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Message;

import android.os.UEventObserver;
import android.util.Slog;
import android.util.Log;
import android.os.Bundle;
import android.os.UserHandle;
import java.lang.String;
import java.lang.Exception;
import android.os.PowerManager;
import android.os.SystemClock;
import android.content.ComponentName;
import android.hardware.ICvbsInManager;

/**
 * 
 */
class CVBSInDeviceService extends ICvbsInManager.Stub 
{
    private static final String TAG = "CVBSInDeviceService";
	private static final String mDevicesPath   = "/devices/virtual/switch/rev_irq";
	private static final String mTvdDevicesPath   = "/devices/virtual/switch/tvd_signal";
	private static final String mDevicesStateKey   = "SWITCH_STATE";
	private static final String mDevicesIndexKey   = "SWITCH_INDEX";
	private static final String mDevicePathKey = "DEVPATH";
    private final Context mContext;
	private int[] mStatus;
	private static final int CVBS_CAMERA_MAX_NUM = 4;
	private static final int CVBS_CAMERA_ID_BASE = 4;//CVBS CAMERA ID BASE  /dev/video4 
	public  static final String ACTION_CAR_REVERSE_STATE= "android.hardware.car.reverse.state";
	public  static final String ACTION_TVD_STATE_CHANGE= "android.hardware.tvd.state.change";
	public static final int CAR_STATE_REVEERING = 1;
	public static final int CAR_STATE_IDLE = 0;
	public static final int TVD_STATE_IN = 1;
	public static final int TVD_STATE_OUT = 0;
	private static final int CAR_REVERSE_STATE_CHANGED = 0;
	private static final int TVD_STATE_CHANGED = 1;
	public static final String INDEX = "index";
	public static final String STATE = "state";

	
    public CVBSInDeviceService(Context context) {
        mContext = context;      
        Log.d(TAG, "CVBSInDeviceService start");
        //mCVBSObserver.startObserving(mDevicesPath);
	    mCVBSObserver.startObserving(mTvdDevicesPath);
		mStatus = new int[CVBS_CAMERA_MAX_NUM];
		for(int i=0;i<CVBS_CAMERA_MAX_NUM;i++)
		{
			mStatus[i] = 0;
		}
    }

	public int getStatus(int index)
	{
		if((index < CVBS_CAMERA_MAX_NUM + CVBS_CAMERA_ID_BASE) && (index>= CVBS_CAMERA_ID_BASE))
		{
			return mStatus[index - CVBS_CAMERA_ID_BASE];
		}
		else
		{
			Log.e(TAG,"index " + index + " beyond of max_num " + CVBS_CAMERA_MAX_NUM);
		}
		return 0;
	}

	 private UEventObserver mCVBSObserver = new UEventObserver() {
       @Override
    	public void onUEvent(UEventObserver.UEvent event) {
        Log.d(TAG, "device change: " + event.toString());
		String path = event.get(mDevicePathKey);
		if(mDevicesPath.equals(path))
		{
	
		}
		else if(mTvdDevicesPath.equals(path))
		{
			String stateString = event.get(mDevicesStateKey);
			int intValue = Integer.parseInt(stateString);
			int cameraid = (intValue >> 8) & 0xff;
			int status = intValue & 0xff;
			Log.e(TAG,"cameraid=" + cameraid + " status=" + status );
			if((cameraid < CVBS_CAMERA_MAX_NUM + CVBS_CAMERA_ID_BASE) && (cameraid >= CVBS_CAMERA_ID_BASE))
			{
				Message message = new Message();      
	           	message.what = TVD_STATE_CHANGED;
				message.arg1 = cameraid;
				message.arg2 = status;
				mStatus[cameraid - CVBS_CAMERA_ID_BASE] = status;
	    	  	mHandler.sendMessage(message);
			}
			
		}
		
    }
    };
	 
	private void updateTVDState(int index,int status)
	{
		Intent intent;
        intent = new Intent(ACTION_TVD_STATE_CHANGE);
        Bundle bundle = new Bundle();
		bundle.putInt(INDEX,index);
		bundle.putInt(STATE,status);
        intent.putExtras(bundle);
       // ActivityManagerNative.broadcastStickyIntent(intent, null, UserHandle.USER_ALL);
       Log.d(TAG,"android.hardware.tvd.state.change");
        mContext.sendBroadcast(intent);
	}
    private void updateCarState(int index,int status) {
		
   	  	Intent intent;
        intent = new Intent(ACTION_CAR_REVERSE_STATE);
        Bundle bundle = new Bundle();
		bundle.putInt(INDEX,index);
		bundle.putInt(STATE,status);
        intent.putExtras(bundle);
       // ActivityManagerNative.broadcastStickyIntent(intent, null, UserHandle.USER_ALL);
        mContext.sendBroadcast(intent);
    }

	
	private final Handler mHandler = new Handler(true /*async*/) {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case CAR_REVERSE_STATE_CHANGED:
                    updateCarState(msg.arg1,msg.arg2);
                    break;
				case TVD_STATE_CHANGED:
					updateTVDState(msg.arg1,msg.arg2);
					break;
            }
        }
    };
	
}

