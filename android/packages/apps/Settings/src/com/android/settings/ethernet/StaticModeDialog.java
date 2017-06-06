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

import com.android.settings.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.security.Credentials;
import android.security.KeyStore;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.EditText;
import android.net.EthernetManager;
import android.net.EthernetDevInfo;
import android.net.InterfaceConfiguration;
import android.os.INetworkManagementService;
import android.util.Log;
import android.net.NetworkUtils;
import android.widget.TextView.OnEditorActionListener;
import android.view.KeyEvent;

import java.net.InetAddress;

public class StaticModeDialog extends AlertDialog implements TextWatcher,
        View.OnClickListener, AdapterView.OnItemSelectedListener {

    private static final String TAG = "StaticIpDialog";
    private final KeyStore mKeyStore = KeyStore.getInstance();
    private final DialogInterface.OnClickListener mListener;
    private EthernetDevInfo mInterfaceInfo;

    private boolean mSecondClick;

    private View mView;
    private Context mContext;

    private EditText mIpaddr;
    private EditText mMask;
    private EditText mDns1;
    private EditText mDns2;
    private EditText mGw;
    private EditText mMacaddr;

    public StaticModeDialog(Context context, DialogInterface.OnClickListener listener,
            EthernetDevInfo interfaceinfo, boolean secondClick) {
        super(context);
        mContext = context;
        mListener = listener;
        mSecondClick = secondClick;
        mInterfaceInfo = interfaceinfo;
    }

    @Override
    protected void onCreate(Bundle savedState) {
        mView = getLayoutInflater().inflate(R.layout.eth_static_dialog, null);
        setTitle(R.string.eth_static_configure);
        setView(mView);
        setInverseBackgroundForced(true);
        Context context = getContext();

        // First, find out all the fields.
        mIpaddr = (EditText) mView.findViewById(R.id.ipaddr_edit);
        mMask = (EditText) mView.findViewById(R.id.netmask_edit);
        mGw = (EditText) mView.findViewById(R.id.gw_edit);
        mDns1 = (EditText) mView.findViewById(R.id.dns1_edit);
        mDns2 = (EditText) mView.findViewById(R.id.dns2_edit);
        mMacaddr= (EditText) mView.findViewById(R.id.macaddr_edit);

       // Second, copy values from the profile.
        mIpaddr.setText(mInterfaceInfo.getIpAddress());
        mMask.setText(mInterfaceInfo.getNetMask());
        mGw.setText(mInterfaceInfo.getGateWay());
        mDns1.setText(mInterfaceInfo.getDns1());
        mDns2.setText(mInterfaceInfo.getDns2());
        String hwAddr = mInterfaceInfo.getHwaddr();
        if(hwAddr == null ) {
            mMacaddr.setText(mContext.getString(R.string.eth_mac_string));
        }else {
            mMacaddr.setText(mInterfaceInfo.getHwaddr().toUpperCase());
        }
        mMacaddr.setEnabled(false);

        mIpaddr.addTextChangedListener(this);
        mMask.addTextChangedListener(this);
        mGw.addTextChangedListener(this);
        mDns1.addTextChangedListener(this);
        mDns2.addTextChangedListener(this);

        setButton(DialogInterface.BUTTON_NEGATIVE, context.getString(R.string.eth_cancel), mListener);
        setButton(DialogInterface.BUTTON_POSITIVE, context.getString(R.string.eth_ok), mListener);
        super.onCreate(savedState);
        mView.findViewById(R.id.eth_conf_editor).setVisibility(View.VISIBLE);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE |
                WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        if(mSecondClick) {
            getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(false);
        } else {
            getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(true);
        }

        mIpaddr.setOnEditorActionListener(new OnEditorActionListener() {
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if(InetAddress.isNumeric(v.getText().toString())) {
                    if(mMask.getText().toString().equals("")) {
                        String mask = getMaskFromIp(mIpaddr.getText().toString());
                        mMask.setText(mask);
                    }
                }
                return false;
            }
        });
    }

    @Override
    public void afterTextChanged(Editable field) {
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(true);
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    public static String getMaskFromIp(String ip) {
        String ary0 = ip.substring(0, ip.indexOf("."));
        Integer itg = Integer.valueOf(ary0);
        int i = itg.intValue();
        if(i < 128 && i > 0) {
            return "255.0.0.0";
        } else if(i < 192) {
            return "255.255.0.0";
        } else if(i < 224) {
            return "255.255.255.0";
        } else {
            return "255.255.255.255";
        }
    }

    @Override
    public void onClick(View v) {
    }

    public EthernetDevInfo getConfigDevInfo() {
        // First, save common fields.
        mInterfaceInfo.setMode(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL);
        mInterfaceInfo.setIpAddress(mIpaddr.getText().toString());
        mInterfaceInfo.setNetMask(mMask.getText().toString());
        mInterfaceInfo.setDns1(mDns1.getText().toString());
        mInterfaceInfo.setDns2(mDns2.getText().toString());
        mInterfaceInfo.setGateWay(mGw.getText().toString());
        return mInterfaceInfo;
    }
}
