/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.server.ethernet;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.IEthernetManager;
import android.net.IEthernetServiceListener;
import android.net.IpConfiguration;
import android.net.IpConfiguration.IpAssignment;
import android.net.IpConfiguration.ProxySettings;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.util.Log;
import android.util.PrintWriterPrinter;
import android.net.DhcpInfo;
import android.net.DhcpResults;
import android.net.LinkProperties;
import android.net.NetworkInfo;
import android.net.NetworkUtils;
import android.net.EthernetManager;
import android.content.ContentResolver;
import android.provider.Settings;
import android.content.Intent;
import android.os.UserHandle;

import com.android.internal.util.IndentingPrintWriter;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.concurrent.atomic.AtomicBoolean;
import java.net.InetAddress;
import java.net.Inet4Address;
import java.io.File;
import java.io.FileInputStream;
import java.io.DataInputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

/**
 * EthernetServiceImpl handles remote Ethernet operation requests by implementing
 * the IEthernetManager interface.
 *
 * @hide
 */
public class EthernetServiceImpl extends IEthernetManager.Stub {
    private static final String TAG = "EthernetServiceImpl";

    private final Context mContext;
    private final EthernetConfigStore mEthernetConfigStore;
    private final AtomicBoolean mStarted = new AtomicBoolean(false);
    private IpConfiguration mIpConfiguration;

    private Handler mHandler;
    private final EthernetNetworkFactory mTracker;
    private final RemoteCallbackList<IEthernetServiceListener> mListeners =
            new RemoteCallbackList<IEthernetServiceListener>();

    public EthernetServiceImpl(Context context) {
        mContext = context;
        Log.i(TAG, "Creating EthernetConfigStore");
        mEthernetConfigStore = new EthernetConfigStore();
        mIpConfiguration = mEthernetConfigStore.readIpAndProxyConfigurations();

        Log.i(TAG, "Read stored IP configuration: " + mIpConfiguration);

        mTracker = new EthernetNetworkFactory(mListeners);
    }

    private void enforceAccessPermission() {
        mContext.enforceCallingOrSelfPermission(
                android.Manifest.permission.ACCESS_NETWORK_STATE,
                "EthernetService");
    }

    private void enforceChangePermission() {
        mContext.enforceCallingOrSelfPermission(
                android.Manifest.permission.CHANGE_NETWORK_STATE,
                "EthernetService");
    }

    private void enforceConnectivityInternalPermission() {
        mContext.enforceCallingOrSelfPermission(
                android.Manifest.permission.CONNECTIVITY_INTERNAL,
                "ConnectivityService");
    }

    public void start() {
        Log.i(TAG, "Starting Ethernet service");

        HandlerThread handlerThread = new HandlerThread("EthernetServiceThread");
        handlerThread.start();
        mHandler = new Handler(handlerThread.getLooper());

        boolean ethEnabled = (getEthernetState() == EthernetManager.ETHERNET_STATE_ENABLED);
        Log.i(TAG, "EthernetService starting up with Ethernet " +
                (ethEnabled ? "enabled" : "disabled"));
        if(ethEnabled) {
            mTracker.start(mContext, mHandler);
        }

        mStarted.set(true);
    }

    /**
     * Get Ethernet configuration
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    @Override
    public IpConfiguration getConfiguration() {
        enforceAccessPermission();

        synchronized (mIpConfiguration) {
            return new IpConfiguration(mIpConfiguration);
        }
    }

    /**
     * Get Ethernet configuration
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    public IpConfiguration getConfigurationByIpAssignment(String type) {
        enforceAccessPermission();
        if(type.equals(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL)) {
            return mEthernetConfigStore.readIpAndProxyConfigurations(IpAssignment.STATIC);
        } else if(type.equals(EthernetManager.ETHERNET_CONNECT_MODE_PPPOE)) {
            return mEthernetConfigStore.readIpAndProxyConfigurations(IpAssignment.PPPOE);
        } else {
            return mEthernetConfigStore.readIpAndProxyConfigurations(IpAssignment.DHCP);
        }
    }

    /**
     * Set Ethernet configuration
     */
    @Override
    public void setConfiguration(IpConfiguration config) {
        if (!mStarted.get()) {
            Log.w(TAG, "System isn't ready enough to change ethernet configuration");
        }

        enforceChangePermission();
        enforceConnectivityInternalPermission();

        synchronized (mIpConfiguration) {
            mEthernetConfigStore.writeIpAndProxyConfigurations(config);
            if(config.ipAssignment == IpAssignment.PPPOE) {
                config.getPppoeConfiguration().saveLoginInfo(getActiveIface());
            }
            // TODO: this does not check proxy settings, gateways, etc.
            // Fix this by making IpConfiguration a complete representation of static configuration.
            if (!config.equals(mIpConfiguration)) {
                mIpConfiguration = new IpConfiguration(config);
                mTracker.stop();
                mTracker.start(mContext, mHandler);
            }
        }
    }

    /**
     * Indicates whether the system currently has one or more
     * Ethernet interfaces.
     */
    @Override
    public boolean isAvailable() {
        enforceAccessPermission();
        return mTracker.isTrackingInterface();
    }

    /**
     * Addes a listener.
     * @param listener A {@link IEthernetServiceListener} to add.
     */
    public void addListener(IEthernetServiceListener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("listener must not be null");
        }
        enforceAccessPermission();
        mListeners.register(listener);
    }

    /**
     * Removes a listener.
     * @param listener A {@link IEthernetServiceListener} to remove.
     */
    public void removeListener(IEthernetServiceListener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("listener must not be null");
        }
        enforceAccessPermission();
        mListeners.unregister(listener);
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        final IndentingPrintWriter pw = new IndentingPrintWriter(writer, "  ");
        if (mContext.checkCallingOrSelfPermission(android.Manifest.permission.DUMP)
                != PackageManager.PERMISSION_GRANTED) {
            pw.println("Permission Denial: can't dump EthernetService from pid="
                    + Binder.getCallingPid()
                    + ", uid=" + Binder.getCallingUid());
            return;
        }

        pw.println("Current Ethernet state: ");
        pw.increaseIndent();
        mTracker.dump(fd, pw, args);
        pw.decreaseIndent();

        pw.println();
        pw.println("Stored Ethernet configuration: ");
        pw.increaseIndent();
        pw.println(mIpConfiguration);
        pw.decreaseIndent();

        pw.println("Handler:");
        pw.increaseIndent();
        mHandler.dump(new PrintWriterPrinter(pw), "EthernetServiceImpl");
        pw.decreaseIndent();
    }

    public LinkProperties getLinkProperties() {
        enforceAccessPermission();
        return mTracker.getLinkProperties();
    }

    public void setEthernetEnabled(boolean enable) {
        Log.d(TAG, "setEthernetEnabled: Enabe=" + enable + " pid=" + Binder.getCallingPid()
                + ", uid=" + Binder.getCallingUid());

        long ident = Binder.clearCallingIdentity();
        try {
            // Save ethernet enable state to database
            final ContentResolver cr = mContext.getContentResolver();
            if (getEthernetState() == EthernetManager.ETHERNET_STATE_ENABLED) {
                if (!enable) {
                    mTracker.stop();
                    Settings.Global.putInt(cr, Settings.Global.ETHERNET_ON, enable ? 1 : 0);
                    Intent intent = new Intent(EthernetManager.ETHERNET_STATE_CHANGED_ACTION);
                    intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                            | Intent.FLAG_RECEIVER_REPLACE_PENDING);
                   intent.putExtra(EthernetManager.EXTRA_ETHERNET_STATE, EthernetManager.ETHERNET_STATE_DISABLED);
                   mContext.sendStickyBroadcastAsUser(intent, UserHandle.ALL);
                }
            } else {
                if (enable) {
                    Settings.Global.putInt(cr, Settings.Global.ETHERNET_ON, enable ? 1 : 0);
                    mTracker.start(mContext, mHandler);
                    Intent intent = new Intent(EthernetManager.ETHERNET_STATE_CHANGED_ACTION);
                    intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                           | Intent.FLAG_RECEIVER_REPLACE_PENDING);
                    intent.putExtra(EthernetManager.EXTRA_ETHERNET_STATE, EthernetManager.ETHERNET_STATE_ENABLED);
                    mContext.sendStickyBroadcastAsUser(intent, UserHandle.ALL);
                }
            }
         } finally {
             Binder.restoreCallingIdentity(ident);
         }
    }

    public int getEthernetState() {
        final ContentResolver cr = mContext.getContentResolver();
        try {
            int state = Settings.Global.getInt(cr, Settings.Global.ETHERNET_ON);
            if (0 == state) {
                return EthernetManager.ETHERNET_STATE_DISABLED;
            } else {
                return EthernetManager.ETHERNET_STATE_ENABLED;
            }
         } catch (Exception ex) {
            Log.e(TAG, "getEthernetState error!");
            return EthernetManager.ETHERNET_STATE_ENABLED;
         }
    }

    /**
     * Return the DHCP-assigned addresses from the last successful DHCP request,
     * if any.
     * @return the DHCP information
     * @deprecated
     */
    public DhcpInfo getDhcpInfo() {
        enforceAccessPermission();
        DhcpResults dhcpResults = mTracker.getDhcpResults();

        DhcpInfo info = new DhcpInfo();

        if (dhcpResults.ipAddress != null &&
                dhcpResults.ipAddress.getAddress() instanceof Inet4Address) {
            info.ipAddress = NetworkUtils.inetAddressToInt((Inet4Address) dhcpResults.ipAddress.getAddress());
            info.netmask = NetworkUtils.prefixLengthToNetmaskInt(dhcpResults.ipAddress.getPrefixLength());
        }

        if (dhcpResults.gateway != null) {
            info.gateway = NetworkUtils.inetAddressToInt((Inet4Address) dhcpResults.gateway);
        }

        int dnsFound = 0;
        for (InetAddress dns : dhcpResults.dnsServers) {
            if (dns instanceof Inet4Address) {
                if (dnsFound == 0) {
                    info.dns1 = NetworkUtils.inetAddressToInt((Inet4Address)dns);
                } else {
                    info.dns2 = NetworkUtils.inetAddressToInt((Inet4Address)dns);
                }
                if (++dnsFound > 1) break;
            }
        }
        InetAddress serverAddress = dhcpResults.serverAddress;
        if (serverAddress instanceof Inet4Address) {
            info.serverAddress = NetworkUtils.inetAddressToInt((Inet4Address)serverAddress);
        }
        info.leaseDuration = dhcpResults.leaseDuration;

        return info;
    }

    public boolean getLinkState() {
        enforceAccessPermission();
        return mTracker.getLinkState();
    }

    public String getActiveIface() {
        enforceAccessPermission();
        return mTracker.getActiveIface();
    }

    /**
     * Fetch NetworkInfo for the network
     */
    public NetworkInfo getNetworkInfo() {
        enforceAccessPermission();
        return mTracker.getNetworkInfo();
    }

    /**
     * @param ifname the string that identifies the network interface
     * check if the interface linkin or not.
     * cat sys/class/net/ethx/carrier 1 means link in,
     * carrier is 0 or doesn't exist means link out.
     * @return {@code true} if linkin
     */
    public boolean checkLink(String ifname) {
        enforceAccessPermission();
        boolean ret = false;
        File filefd = null;
        FileInputStream fstream = null;
        String s = null;
        String SYS_NET = "/sys/class/net/";
        try {
            if(!(new File(SYS_NET + ifname).exists()))
                return false;
            fstream = new FileInputStream(SYS_NET + ifname + "/carrier");
            DataInputStream in = new DataInputStream(fstream);
            BufferedReader br = new BufferedReader(new InputStreamReader(in));

            s = br.readLine();
        } catch (IOException ex) {
            Log.e(TAG, "checkLink error: " + ex);
        } finally {
            if (fstream != null) {
                try {
                    fstream.close();
                } catch (IOException ex) {
                    Log.e(TAG, "checkLink fstream.close " + ex);
                }
            }
        }
        if(s != null && s.equals("1")) {
            ret = true;
        }
        Log.d(TAG, "checkLink: current link state " + (ret ? "link in" : "link out"));
        return ret;
    }
}
