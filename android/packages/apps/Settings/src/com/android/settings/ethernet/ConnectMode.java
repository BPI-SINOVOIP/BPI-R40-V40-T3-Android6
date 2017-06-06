///*
// * Copyright (C) 2013 The Android Open Source Project
// *
// * Licensed under the Apache License, Version 2.0 (the "License");
// * you may not use this file except in compliance with the License.
// * You may obtain a copy of the License at
// *
// *      http://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing, software
// * distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// */
//
//package com.android.settings.ethernet;
//
//import android.preference.PreferenceScreen;
//import android.provider.Settings;
//import android.content.pm.PackageManager;
//import com.android.settings.R;
//import android.content.Context;
//
///**
// * A page with 3 radio buttons to choose the location mode.
// *
// * There are 3 location modes when location access is enabled:
// *
// * High accuracy: use both GPS and network location.
// *
// * Battery saving: use network location only to reduce the power consumption.
// *
// * Sensors only: use GPS location only.
// */
//public class ConnectMode extends LocationSettingsBase
//        implements RadioButtonPreference.OnClickListener {
//    private static final String KEY_DHCP_MODE = "dhcp_mode";
//    private RadioButtonPreference mDhcp;
//    private static final String KEY_STATIC_MODE = "static_mode";
//    private RadioButtonPreference mStatic;
//    private static final String KEY_PPPOE_MODE = "pppoe_mode";
//    private RadioButtonPreference mPppoe;
//
//    @Override
//    public void onResume() {
//        super.onResume();
//        createPreferenceHierarchy();
//    }
//
//    @Override
//    public void onPause() {
//        super.onPause();
//    }
//
//    private PreferenceScreen createPreferenceHierarchy() {
//        PreferenceScreen root = getPreferenceScreen();
//        if (root != null) {
//            root.removeAll();
//        }
//        addPreferencesFromResource(R.xml.eth_connect_mode);
//        root = getPreferenceScreen();
//
//        mDhcp = (RadioButtonPreference) root.findPreference(KEY_DHCP_MODE);
//        mStatic = (RadioButtonPreference) root.findPreference(KEY_STATIC_MODE);
//        mPppoe = (RadioButtonPreference) root.findPreference(KEY_PPPOE_MODE);
//        return root;
//    }
//
//    private void updateRadioButtons(RadioButtonPreference activated) {
//        if (activated == null) {
//            mDhcp.setChecked(false);
//            mStatic.setChecked(false);
//            mPppoe.setChecked(false);
//        } else if (activated == mDhcp) {
//            mDhcp.setChecked(true);
//            mStatic.setChecked(false);
//            mPppoe.setChecked(false);
//        } else if (activated == mStatic) {
//            mDhcp.setChecked(false);
//            mStatic.setChecked(true);
//            mPppoe.setChecked(false);
//        } else if (activated == mPppoe) {
//            mDhcp.setChecked(false);
//            mStatic.setChecked(false);
//            mPppoe.setChecked(true);
//        }
//    }
//
//    @Override
//    public void onRadioButtonClicked(RadioButtonPreference emiter) {
//        int mode = Settings.Secure.ETH_CONNECT_MODE_DHCP;
//        if (emiter == mDhcp) {
//            mode = Settings.Secure.ETH_CONNECT_MODE_DHCP;
//        } else if (emiter == mStatic) {
//            mode = Settings.Secure.ETH_CONNECT_MODE_STATIC;
//        } else if (emiter == mPppoe) {
//            mode = Settings.Secure.ETH_CONNECT_MODE_PPPOE;
//        }
//    }
//
//    @Override
//    public void onModeChanged(int mode, boolean restricted) {
//        switch (mode) {
//            case Settings.Secure.ETH_CONNECT_MODE_DHCP:
//                updateRadioButtons(mDhcp);
//                break;
//            case Settings.Secure.ETH_CONNECT_MODE_STATIC:
//                updateRadioButtons(mStatic);
//                break;
//            case Settings.Secure.ETH_CONNECT_MODE_PPPOE:
//                updateRadioButtons(mPppoe);
//                break;
//            default:
//                break;
//        }
//        mHighAccuracy.setEnabled(enabled);
//        mBatterySaving.setEnabled(enabled);
//        mSensorsOnly.setEnabled(enabled);
//    }
//
//    @Override
//    public int getHelpResource() {
//    }
//}
