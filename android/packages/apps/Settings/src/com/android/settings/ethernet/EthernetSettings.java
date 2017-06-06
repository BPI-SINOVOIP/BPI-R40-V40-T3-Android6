/*
 * Copyright (C) 2011 The Android Open Source Project
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

import static android.net.EthernetManager.ETHERNET_STATE_DISABLED;
import static android.net.EthernetManager.ETHERNET_STATE_ENABLED;

import com.android.settings.R;

import android.app.Dialog;
import android.app.AlertDialog;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.BroadcastReceiver;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.LinkProperties;
import android.net.LinkAddress;
import android.net.EthernetManager;
import android.net.EthernetDevInfo;
import android.net.InterfaceConfiguration;
import android.os.INetworkManagementService;
import android.net.RouteInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ServiceManager;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceGroup;
import android.preference.PreferenceCategory;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.util.Slog;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.content.ContentResolver;
import com.android.settings.SettingsPreferenceFragment;
import android.provider.Settings;
import com.android.settings.SettingsActivity;
import com.android.internal.logging.MetricsLogger;
import com.android.settings.InstrumentedFragment;
import com.android.settings.AccessiblePreferenceCategory;
import com.android.settings.Utils;
import com.android.settings.users.UserDialogs;



import java.net.Inet4Address;
//import java.nio.charset.Charsets;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;
import android.os.AsyncTask;
import android.widget.TextView;
import android.widget.Switch;
import android.app.ActionBar;
import android.view.Gravity;
import android.widget.Toast;
import java.net.InetAddress;
import java.lang.reflect.Field;

public class EthernetSettings extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener, RadioButtonPreference.OnClickListener,
        DialogInterface.OnClickListener, DialogInterface.OnDismissListener {

    private static final String TAG = "EthernetSettings";
    private static boolean DBG = true;

    private ConnectivityManager mService;
    private EthernetEnabler mEthEnabler;

//*******************************************************//
    private static final String KEY_TOGGLE = "eth_toggle";
    private static final String KEY_DEVICES_TITLE = "eth_device_title";
    private static final String KEY_ETH_CONFIG = "eth_configure";
    private static final String KEY_IP_PRE = "current_ip_address";
    private static final String KEY_MAC_PRE = "mac_address";
    private static final String KEY_GW_PRE = "gw_address";
    private static final String KEY_DNS1_PRE = "dns1_address";
    private static final String KEY_DNS2_PRE = "dns2_address";
    private static final String KEY_MASK_PRE = "mask_address";
    private static final String KEY_DHCP_MODE = "dhcp_mode";
    private static final String KEY_STATIC_MODE = "static_mode";
    private static final String KEY_PPPOE_MODE = "pppoe_mode";
    private static final String KEY_CONNECT_MODE = "connect_mode";
    private static final String KEY_ETH_INFO = "eth_info";

    private static final String DHCP_MODE = EthernetManager.ETHERNET_CONNECT_MODE_DHCP;
    private static final String STATIC_MODE = EthernetManager.ETHERNET_CONNECT_MODE_MANUAL;
    private static final String PPPOE_MODE = EthernetManager.ETHERNET_CONNECT_MODE_PPPOE;

    private Preference mMacPreference;
    private Preference mIpPreference;
    private Preference mGwPreference;
    private Preference mDns1Preference;
    private Preference mDns2Preference;
    private Preference mMaskPreference;
    private RadioButtonPreference mDhcpMode;
    private RadioButtonPreference mStaticMode;
  //  private RadioButtonPreference mPppoeMode;
    private PreferenceCategory mConnectMode;
    private PreferenceCategory mEthInfo;

//    private EthPreference mSelected;
    private EthernetManager mEthManager;
    private final IntentFilter mFilter;
    private final BroadcastReceiver mEthStateReceiver;
    private List<EthernetDevInfo> mListDevices = new ArrayList<EthernetDevInfo>();
    private StaticModeDialog mStaticDialog;
    private PppoeModeDialog mPppoeDialog;
    private DhcpModeDialog mDhcpDialog;
    private TextView mEmptyView;

    private String currentMode;
    private String prevMode;
    private boolean waitForConnectResult = false;
    private boolean waitForDisconnectResult = false;
    /*  New a BroadcastReceiver for EthernetSettings  */
    public EthernetSettings() {
        mFilter = new IntentFilter();
        mFilter.addAction(EthernetManager.ETHERNET_STATE_CHANGED_ACTION);
        mFilter.addAction(EthernetManager.NETWORK_STATE_CHANGED_ACTION);
        mEthStateReceiver = new BroadcastReceiver() {
           @Override
           public void onReceive(Context context, Intent intent) {
               handleEvent(context, intent);
           }
        };
    }
    
 	  @Override
    protected int getMetricsCategory() {
        return MetricsLogger.WIFI;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.ethernet_settings);
        mIpPreference = findPreference(KEY_IP_PRE);
        mMacPreference = findPreference(KEY_MAC_PRE);
        mGwPreference = findPreference(KEY_GW_PRE);
        mDns1Preference = findPreference(KEY_DNS1_PRE);
        mDns2Preference = findPreference(KEY_DNS2_PRE);
        mMaskPreference = findPreference(KEY_MASK_PRE);
        mDhcpMode = (RadioButtonPreference) findPreference(KEY_DHCP_MODE);
        mDhcpMode.setOnClickListener(this);
        mStaticMode = (RadioButtonPreference) findPreference(KEY_STATIC_MODE);
        mStaticMode.setOnClickListener(this);
//        mPppoeMode = (RadioButtonPreference) findPreference(KEY_PPPOE_MODE);
 //       mPppoeMode.setOnClickListener(this);
        mConnectMode = (PreferenceCategory) findPreference(KEY_CONNECT_MODE);
        mEthInfo = (PreferenceCategory) findPreference(KEY_ETH_INFO);

        /* first, we must get the Service. */
        mService = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        mEthManager = EthernetManager.getInstance();
        if(DBG) Slog.d(TAG, "onCreate.");

        /* Get the SaveConfig and update for Dialog. */
        EthernetDevInfo ethDevInfo = mEthManager.getDevInfo();
        currentMode = mEthManager.getEthernetMode();
        prevMode = currentMode;
        updateRadioButtons(currentMode);
        if(ethDevInfo != null) {
            updateEthernetInfo(ethDevInfo);
        } else {
            updateEthernetInfo(null);
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mEmptyView = (TextView) getView().findViewById(android.R.id.empty);
        getListView().setEmptyView(mEmptyView);

        if(mEthManager.getEthernetState() == EthernetManager.ETHERNET_STATE_DISABLED) {
            if(mEmptyView != null) {
                mEmptyView.setText(R.string.eth_off);
            }
            getPreferenceScreen().removeAll();
        } else {
            addAllPreference();
        }
    }

    private void updateRadioButtons(String mode) {
        if (mode.equals(DHCP_MODE)) {
            mDhcpMode.setChecked(true);
            mStaticMode.setChecked(false);
            //mPppoeMode.setChecked(false);
        } else if (mode.equals(STATIC_MODE)) {
            mDhcpMode.setChecked(false);
            mStaticMode.setChecked(true);
            //mPppoeMode.setChecked(false);
        } else if (mode.equals(PPPOE_MODE)) {
            mDhcpMode.setChecked(false);
            mStaticMode.setChecked(false);
            //mPppoeMode.setChecked(true);
        }
    }

    private boolean getIpoeState() {
        final ContentResolver cr = getContentResolver();
        try {
            return false;
        } catch (Exception ex) {
            return false;
        }
    }

    @Override
    public void onRadioButtonClicked(RadioButtonPreference emiter) {
        if(emiter == mDhcpMode) {
            currentMode = DHCP_MODE;
            mDhcpDialog = new DhcpModeDialog(getActivity(), this, mEthManager.getLoginInfo(DHCP_MODE),
                    getIpoeState(), mEthManager.getEthernetMode().equals(DHCP_MODE));
            mDhcpDialog.setOnDismissListener(this);
            mDhcpDialog.show();
        } else if(emiter == mStaticMode) {
            currentMode = STATIC_MODE;
            EthernetDevInfo devInfo = mEthManager.getStaticConfig();
            if(devInfo.getIpAddress() == null) {
                devInfo = mEthManager.getDevInfo();
            }
            mStaticDialog = new StaticModeDialog(getActivity(), this, devInfo,
                    mEthManager.getEthernetMode().equals(STATIC_MODE));
            mStaticDialog.setOnDismissListener(this);
            mStaticDialog.show();
        } /*else if(emiter == mPppoeMode) {
            currentMode = PPPOE_MODE;
            mPppoeDialog = new PppoeModeDialog(getActivity(), this, mEthManager.getLoginInfo(PPPOE_MODE),
                    mEthManager.getEthernetMode().equals(PPPOE_MODE));
            mPppoeDialog.setOnDismissListener(this);
            mPppoeDialog.show();
        }*/
        updateRadioButtons(currentMode);
    }

    @Override
    public void onSaveInstanceState(Bundle savedState) {
    }

    @Override
    public void onStart() {
        super.onStart();
        final SettingsActivity activity = (SettingsActivity) getActivity();
        mEthEnabler = new EthernetEnabler(activity, activity.getSwitchBar());

    }

    @Override
    public void onStop() {
        super.onStop();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mEthEnabler != null) {
            mEthEnabler.resume();
        }
        getActivity().registerReceiver(mEthStateReceiver, mFilter);
    }

    @Override
    public void onPause() {
        if (mEthEnabler != null) {
            mEthEnabler.pause();
        }
        getActivity().unregisterReceiver(mEthStateReceiver);
        super.onPause();
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        // Here is the exit of a dialog.
        //mDialog = null;
        updateRadioButtons(prevMode);
    }

    private void displayToast(String str) {
        Toast.makeText((Context)getActivity(), str, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichbutton) {
        /* get the information form dialog. */
        EthernetDevInfo configDevInfo = null;
        if (whichbutton == DialogInterface.BUTTON_POSITIVE) {
            Slog.d(TAG, "onClick: BUTTON_POSITIVE");
            if(currentMode.equals(DHCP_MODE)) {
                configDevInfo = mDhcpDialog.getConfigDevInfo();
            } else if(currentMode.equals(STATIC_MODE)) {
                configDevInfo = mStaticDialog.getConfigDevInfo();
                try {
                    Field field = mStaticDialog.getClass().getSuperclass().
                            getSuperclass().getDeclaredField("mShowing");
                    field.setAccessible(true);
                    if(!InetAddress.isNumeric(configDevInfo.getIpAddress())) {
                        displayToast("IP Address is invalid !");
                        Log.e(TAG,"IP Address is invalid !");
                        field.set(mStaticDialog, false);
                        return;
                    } else {
                        field.set(mStaticDialog, true);
                    }

                    if(!InetAddress.isNumeric(configDevInfo.getNetMask())) {
                        displayToast("Netmask Address is invalid !");
                        Log.e(TAG,"Netmask Address is invalid !");
                        field.set(mStaticDialog, false);
                        return;
                    } else {
                        field.set(mStaticDialog, true);
                    }
                } catch (NoSuchFieldException ne) {
                } catch (IllegalAccessException ie) {
                }
            } else if(currentMode.equals(PPPOE_MODE)) {
                configDevInfo = mPppoeDialog.getConfigDevInfo();
            }

            if(!prevMode.equals(currentMode)) {
                prevMode = currentMode;
            }
            setMode(currentMode, configDevInfo);
        } else if(whichbutton == DialogInterface.BUTTON_NEGATIVE) {
//            updateRadioButtons(prevMode);
            if(currentMode.equals(STATIC_MODE)) {
                try {
                    Field field = mStaticDialog.getClass().getSuperclass().
                    getSuperclass().getDeclaredField("mShowing");
                    field.setAccessible(true);
                    field.set(mStaticDialog, true);
                } catch (NoSuchFieldException ne) {
                } catch (IllegalAccessException ie) {
                }
            }
            Slog.d(TAG, "onClick: BUTTON_NEUTRAL");
        }
    }

    private void setMode(final String mode, final EthernetDevInfo devInfo) {
        Thread setModeThread = new Thread(new Runnable() {
            public void run() {
                Log.d(TAG, "Start setMode thread!");
                mEthManager.setEthernetMode(mode, devInfo);
            }
        });
        setModeThread.start();
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo info) {
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        return false;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue){
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen screen, Preference preference) {
        return true;
    }

    private void updateEthernetInfo(EthernetDevInfo DevIfo) {
        String ifname = "";

        if(DevIfo != null) {
            if(DevIfo.getHwaddr() != null) {
                mMacPreference.setSummary(DevIfo.getHwaddr().toUpperCase());
            }
            mIpPreference.setSummary(DevIfo.getIpAddress());
            mGwPreference.setSummary(DevIfo.getGateWay());
            mDns1Preference.setSummary(DevIfo.getDns1());
            mDns2Preference.setSummary(DevIfo.getDns2());
            mMaskPreference.setSummary(DevIfo.getNetMask());
        } else {
            mIpPreference.setSummary(getString(R.string.eth_ip_string));
            mGwPreference.setSummary(getString(R.string.eth_gw_string));
            mDns1Preference.setSummary(getString(R.string.eth_dns1_string));
            mDns2Preference.setSummary(getString(R.string.eth_dns2_string));
            mMaskPreference.setSummary(getString(R.string.eth_mask_string));
        }
    }

    private void handleEvent(Context context, Intent intent) {
        String action = intent.getAction();
        if (EthernetManager.NETWORK_STATE_CHANGED_ACTION.equals(action)) {
	    
            final int event = intent.getIntExtra(EthernetManager.EXTRA_ETHERNET_STATE,
                    EthernetManager.EVENT_ETHERNET_CONNECT_SUCCESSED);
            switch(event) {
                case EthernetManager.EVENT_ETHERNET_CONNECT_SUCCESSED:
                case EthernetManager.EVENT_PPPOE_CONNECT_SUCCESSED:
                    EthernetDevInfo devInfo = mEthManager.getDevInfo();
                    updateEthernetInfo(devInfo);
                    break;
                case EthernetManager.EVENT_ETHERNET_DISCONNECT_SUCCESSED:
                case EthernetManager.EVENT_PPPOE_DISCONNECT_SUCCESSED:
                    updateEthernetInfo(null);
                    break;
                default:
                    break;
            }
        } else if(EthernetManager.ETHERNET_STATE_CHANGED_ACTION.equals(action)) {
            final int event = intent.getIntExtra(EthernetManager.EXTRA_ETHERNET_STATE,
                    EthernetManager.EVENT_INTERFACE_ADDED);

            switch(event) {
                case EthernetManager.EVENT_INTERFACE_ADDED:
                    mMacPreference.setSummary(mEthManager.getDevInfo().getHwaddr());
                    break;
                case EthernetManager.EVENT_INTERFACE_REMOVED:
                    mMacPreference.setSummary(context.getString(R.string.eth_mac_string));
                    break;
                case EthernetManager.ETHERNET_STATE_DISABLED:
                    if(mEmptyView != null) {
                        mEmptyView.setText(R.string.eth_off);
                    }
                    getPreferenceScreen().removeAll();
                    break;
                case EthernetManager.ETHERNET_STATE_ENABLED:
                    addAllPreference();
                    break;
                default:
                    break;
            }
        }

    }

    private void addAllPreference() {
        getPreferenceScreen().addPreference(mMacPreference);
        getPreferenceScreen().addPreference(mIpPreference);
        getPreferenceScreen().addPreference(mGwPreference);
        getPreferenceScreen().addPreference(mDns1Preference);
        getPreferenceScreen().addPreference(mDns2Preference);
        getPreferenceScreen().addPreference(mMaskPreference);
        getPreferenceScreen().addPreference(mDhcpMode);
        getPreferenceScreen().addPreference(mStaticMode);
       // getPreferenceScreen().addPreference(mPppoeMode);
        getPreferenceScreen().addPreference(mConnectMode);
        getPreferenceScreen().addPreference(mEthInfo);
    }
}
