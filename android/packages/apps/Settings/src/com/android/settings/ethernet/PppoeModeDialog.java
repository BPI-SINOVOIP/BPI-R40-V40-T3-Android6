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

import java.net.InetAddress;

public class PppoeModeDialog extends AlertDialog implements TextWatcher,
        View.OnClickListener, AdapterView.OnItemSelectedListener {

    private static final String TAG = "PppoeDialog";
    private final KeyStore mKeyStore = KeyStore.getInstance();
    private final DialogInterface.OnClickListener mListener;
    private EthernetDevInfo mInterfaceInfo;

    private boolean mSecondClick;

    private View mView;

    private EditText mUser;
    private EditText mPassword;

    public PppoeModeDialog(Context context, DialogInterface.OnClickListener listener,
            EthernetDevInfo interfaceinfo, boolean secondClick) {
        super(context);
        mListener = listener;
        mSecondClick = secondClick;
        mInterfaceInfo = interfaceinfo;
    }

    @Override
    protected void onCreate(Bundle savedState) {
        mView = getLayoutInflater().inflate(R.layout.eth_pppoe_dialog, null);
        setTitle(R.string.eth_pppoe_configure);
        setView(mView);
        setInverseBackgroundForced(true);
        Context context = getContext();

        // First, find out all the fields.
        mUser = (EditText) mView.findViewById(R.id.pppoe_user_edit);
        mPassword = (EditText) mView.findViewById(R.id.pppoe_password_edit);

        mUser.setText(mInterfaceInfo.getUsername());
        mPassword.setText(mInterfaceInfo.getPasswd());

        mUser.addTextChangedListener(this);
        mPassword.addTextChangedListener(this);

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
    }

    @Override
    public void afterTextChanged(Editable field) {
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    @Override
    public void onClick(View v) {
    }

    public EthernetDevInfo getConfigDevInfo() {
        // First, save common fields.
        mInterfaceInfo.setMode(EthernetManager.ETHERNET_CONNECT_MODE_PPPOE);
        mInterfaceInfo.setUsername(mUser.getText().toString());
        mInterfaceInfo.setPasswd(mPassword.getText().toString());
        return mInterfaceInfo;
    }
}
