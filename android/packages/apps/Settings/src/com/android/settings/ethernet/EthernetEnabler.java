/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.settings.ethernet;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.net.NetworkInfo;
import android.net.wifi.SupplicantState;
import android.os.UserHandle;
import android.provider.Settings;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.Toast;
import android.net.EthernetManager;
import android.util.Log;
import com.android.settings.widget.SwitchBar;

public class EthernetEnabler implements SwitchBar.OnSwitchChangeListener  {
    private final Context mContext;
    private SwitchBar mSwitchBar;
    private boolean mListeningToOnSwitchChange = false;

    private final EthernetManager mEthernetManager;
    private final IntentFilter mIntentFilter;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if(EthernetManager.ETHERNET_STATE_CHANGED_ACTION.equals(action)) {
                final int event = intent.getIntExtra(EthernetManager.EXTRA_ETHERNET_STATE,
                        EthernetManager.EVENT_INTERFACE_ADDED);
                switch(event) {
                    case EthernetManager.ETHERNET_STATE_ENABLED:
                        mSwitchBar.setChecked(true);
                        mSwitchBar.setEnabled(true);
                        break;
                    case EthernetManager.ETHERNET_STATE_DISABLED:
                        mSwitchBar.setChecked(false);
                        mSwitchBar.setEnabled(true);
                        break;
                    default:
                        break;
                }
            }
        }
    };

    public EthernetEnabler(Context context, SwitchBar switchBar) {
        mContext = context;
        mSwitchBar = switchBar;
        mEthernetManager = EthernetManager.getInstance();
        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(EthernetManager.ETHERNET_STATE_CHANGED_ACTION);
        setupSwitchBar();
    }

    public void setupSwitchBar() {
        final int state = mEthernetManager.getEthernetState();
        if (!mListeningToOnSwitchChange) {
            mSwitchBar.addOnSwitchChangeListener(this);
            mListeningToOnSwitchChange = true;
        }
        if(EthernetManager.ETHERNET_STATE_ENABLED == state) {
            mSwitchBar.setChecked(true);
        } else {
            mSwitchBar.setChecked(false);
        }
        mSwitchBar.show();
    }

    public void teardownSwitchBar() {
        if (mListeningToOnSwitchChange) {
            mSwitchBar.removeOnSwitchChangeListener(this);
            mListeningToOnSwitchChange = false;
        }
        mSwitchBar.hide();
    }

    public void resume() {
        mContext.registerReceiver(mReceiver, mIntentFilter);
        if (!mListeningToOnSwitchChange) {
            mSwitchBar.addOnSwitchChangeListener(this);
            mListeningToOnSwitchChange = true;
        }
    }

    public void pause() {
        mContext.unregisterReceiver(mReceiver);
        if (mListeningToOnSwitchChange) {
            mSwitchBar.removeOnSwitchChangeListener(this);
            mListeningToOnSwitchChange = false;
        }
    }

    @Override
    public void onSwitchChanged(Switch switchView, boolean isChecked) {
        mSwitchBar.setEnabled(false);
        setEnabled(isChecked);
    }

    private void setEnabled(final boolean enable) {
        final int ethernetState = mEthernetManager.getEthernetState();
        boolean isEnabled = ethernetState == mEthernetManager.ETHERNET_STATE_ENABLED;
        if(enable == isEnabled) {
            mSwitchBar.setEnabled(true);
            return;
        }
        Thread setEnabledThread = new Thread(new Runnable() {
            public void run() {
                mEthernetManager.setEthernetEnabled(enable);
            }
        });
        setEnabledThread.start();
    }
}
