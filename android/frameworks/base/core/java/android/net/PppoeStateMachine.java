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

package android.net;

import com.android.internal.util.Protocol;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.DhcpResults;
import android.net.NetworkUtils;
import android.os.Message;
import android.util.Log;

/**
 * StateMachine that interacts with the native pppoe client and can talk to
 * a controller that also needs to be a StateMachine

 * @hide
 */
public class PppoeStateMachine extends StateMachine {

    private static final String TAG = "PppoeStateMachine";
    private static final boolean DBG = true;


    /* A StateMachine that controls the PppoeStateMachine */
    private StateMachine mController;

    private Context mContext;

    //Remember PPPoE configuration from first request
    private DhcpResults mDhcpResults;

    private static final String ACTION_DHCP_RENEW = "android.net.wifi.DHCP_RENEW";
    private final String mInterfaceName;
    private boolean mRegisteredForPrePppoeNotification = false;

    private static final int BASE = Protocol.BASE_PPPOE;

    public static final int CMD_START_PPPOE                  = BASE + 1;
    public static final int CMD_STOP_PPPOE                   = BASE + 2;
    public static final int CMD_PRE_PPPOE_ACTION             = BASE + 3;
    public static final int CMD_POST_PPPOE_ACTION            = BASE + 4;
    public static final int CMD_ON_QUIT                      = BASE + 5;
    public static final int CMD_PRE_PPPOE_ACTION_COMPLETE    = BASE + 6;

    /* Message.arg1 arguments to CMD_POST_PPPOE_ACTION notification */
    public static final int PPPOE_SUCCESS =  11;
    public static final int PPPOE_FAILURE =  12;
    public static final int PPPOE_STOPPED =  13;

    private State mDefaultState = new DefaultState();
    private State mStoppedState = new StoppedState();
    private State mWaitBeforeStartState = new WaitBeforeStartState();
    private State mRunningState = new RunningState();

    private PppoeStateMachine(Context context, StateMachine controller, String intf) {
    	super("PppoeStateMachine");

        mContext = context;
        mController = controller;
        mInterfaceName = intf;

        addState(mDefaultState);
            addState(mStoppedState, mDefaultState);
            addState(mWaitBeforeStartState, mDefaultState);
            addState(mRunningState, mDefaultState);

        setInitialState(mStoppedState);
    }

    public static PppoeStateMachine makePppoeStateMachine(Context context, StateMachine controller,
            String intf) {
        PppoeStateMachine dsm = new PppoeStateMachine(context, controller, intf);
        dsm.start();
        return dsm;
    }

    public void registerForPrePppoeNotification() {
        mRegisteredForPrePppoeNotification = true;
    }

    /**
     * Quit the PppoeStateMachine.
     *
     * @hide
     */
    public void doQuit() {
        quit();
    }

    protected void onQuitting() {
        mController.sendMessage(CMD_ON_QUIT);
    }

    class DefaultState extends State {
        @Override
        public boolean processMessage(Message message) {
            if (DBG) Log.d(TAG, getName() + message.toString() + "\n");
            switch (message.what) {
                default:
                    Log.e(TAG, "Error! unhandled message  " + message);
                    break;
            }
            return HANDLED;
        }
    }


    class StoppedState extends State {
        @Override
        public void enter() {
            if (DBG) Log.d(TAG, getName() + "\n");
        }

        @Override
        public boolean processMessage(Message message) {
            boolean retValue = HANDLED;
            if (DBG) Log.d(TAG, getName() + message.toString() + "\n");
            switch (message.what) {
                case CMD_START_PPPOE:
                    if (mRegisteredForPrePppoeNotification) {
                        mController.sendMessage(CMD_PRE_PPPOE_ACTION);
                        transitionTo(mWaitBeforeStartState);
                    } else {
                    	if (startPppoe(mInterfaceName)) {
                            transitionTo(mRunningState);
                        }
                    }
                    break;
                case CMD_STOP_PPPOE:
                    //ignore
                    break;
                default:
                    retValue = NOT_HANDLED;
                    break;
            }
            return retValue;
        }
    }

    class WaitBeforeStartState extends State {
        @Override
        public void enter() {
            if (DBG) Log.d(TAG, getName() + "\n");
        }

        @Override
        public boolean processMessage(Message message) {
            boolean retValue = HANDLED;
            if (DBG) Log.d(TAG, getName() + message.toString() + "\n");
            switch (message.what) {
                case CMD_PRE_PPPOE_ACTION_COMPLETE:
                    if (startPppoe(mInterfaceName)) {
                        transitionTo(mRunningState);
                    } else {
                        transitionTo(mStoppedState);
                    }
                    break;
                case CMD_STOP_PPPOE:
                    transitionTo(mStoppedState);
                    break;
                case CMD_START_PPPOE:
                    //ignore
                    break;
                default:
                    retValue = NOT_HANDLED;
                    break;
            }
            return retValue;
        }
    }

    class RunningState extends State {
        @Override
        public void enter() {
            if (DBG) Log.d(TAG, getName() + "\n");
        }

        @Override
        public boolean processMessage(Message message) {
            boolean retValue = HANDLED;
            if (DBG) Log.d(TAG, getName() + message.toString() + "\n");
            switch (message.what) {
                case CMD_STOP_PPPOE:
                    if (!NetworkUtils.stopPppoe(mInterfaceName)) {
                        Log.e(TAG, "Failed to stop PPPoE on " + mInterfaceName);
                    }
                    mController.obtainMessage(CMD_POST_PPPOE_ACTION, PPPOE_STOPPED, 0);
                    transitionTo(mStoppedState);
                    break;
                case CMD_START_PPPOE:
                    //ignore
                    break;
                default:
                    retValue = NOT_HANDLED;
            }
            return retValue;
        }
    }

    private boolean startPppoe(String interfaceName) {
        boolean success = false;
        DhcpResults dhcpResults = new DhcpResults();
        NetworkUtils.stopPppoe(mInterfaceName);
        if (DBG) Log.d(TAG, "Start PPPoE on " + mInterfaceName);
        success = NetworkUtils.startPppoe(mInterfaceName, "test1", dhcpResults);
        if (success) {
            if (DBG) Log.d(TAG, "Start PPPoE succeeded on " + mInterfaceName);
            mDhcpResults = dhcpResults;
            Log.d(TAG, "mDhcpResults =  " + mDhcpResults.toString());
            mController.obtainMessage(CMD_POST_PPPOE_ACTION, PPPOE_SUCCESS, 0, dhcpResults)
                .sendToTarget();
        } else {
            Log.e(TAG, "start PPPoE failed on " + mInterfaceName + ": " +
                    NetworkUtils.getPppoeError());
            NetworkUtils.stopPppoe(mInterfaceName);
            mController.obtainMessage(CMD_POST_PPPOE_ACTION, PPPOE_FAILURE, 0)
                .sendToTarget();
        }
        return success;
    }
}