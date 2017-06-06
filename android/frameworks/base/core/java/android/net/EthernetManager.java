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

package android.net;

import android.annotation.SdkConstant;
import android.annotation.SdkConstant.SdkConstantType;

import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.content.Context;
import android.util.Log;
import android.net.IEthernetManager;
import android.net.IEthernetServiceListener;
import android.net.IpConfiguration;
import android.net.IpConfiguration.IpAssignment;
import android.net.StaticIpConfiguration;
import android.net.PppoeConfiguration;
import android.net.LinkProperties;
import android.net.DhcpInfo;
import android.net.NetworkUtils;
import android.content.ContentResolver;
import android.provider.Settings;
import android.os.Binder;
import android.net.NetworkInfo;

import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;

import java.util.Iterator;
import java.util.List;
import java.net.InetAddress;
import java.net.Inet4Address;
import java.util.ArrayList;

/**
 * This class provides the primary API for managing all aspects of Ethernet
 * connectivity. Get an instance of this class by calling
 * {@link android.content.Context#getSystemService(String) Context.getSystemService(Context.ETHERNET_SERVICE)}.
 *
 * This is the API to use when performing Ethernet specific operations. To
 * perform operations that pertain to network connectivity at an abstract
 * level, use {@link android.net.ConnectivityManager}.
 */
public class EthernetManager {
    private static final String TAG = "AWEthernetManager";
    private static final int MSG_AVAILABILITY_CHANGED = 1000;

    /**
     * Network connect mode
     */
    public static final String ETHERNET_CONNECT_MODE_DHCP       = "dhcp";
    public static final String ETHERNET_CONNECT_MODE_MANUAL     = "manual";
    public static final String ETHERNET_CONNECT_MODE_PPPOE      = "pppoe";

    /**
     * Broadcast intent action indicating that Ethernet linkup/down
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ETHERNET_STATE_CHANGED_ACTION =
            "android.net.ethernet.ETHERNET_STATE_CHANGE";


    /**
     * Broadcast intent action indicating that Ethernet has been conncted/disconnected
     *
     * One extra provides network info.
     * Another extra provides link property.
     * Broadcast by EthernetStateTracker.
     *
     * @see #EXTRA_NETWORK_INFO
     * @see #EXTRA_LINK_PROPERTIES
     */
    public static final String NETWORK_STATE_CHANGED_ACTION =
            "android.net.ethernet.STATE_CHANGE";

    public static final int ETHERNET_STATE_DISABLED         = 0;
    public static final int ETHERNET_STATE_ENABLED          = 1;
    public static final int ETHERNET_STATE_UNKNOWN          = 2;

    public static final int EVENT_ETHERNET_CONNECT_FAILED       = 0;
    public static final int EVENT_ETHERNET_CONNECT_SUCCESSED    = 1;
    public static final int EVENT_ETHERNET_DISCONNECT_FAILED    = 2;
    public static final int EVENT_ETHERNET_DISCONNECT_SUCCESSED = 3;
    public static final int EVENT_PPPOE_CONNECT_FAILED          = 20;
    public static final int EVENT_PPPOE_CONNECT_SUCCESSED       = 21;
    public static final int EVENT_PPPOE_DISCONNECT_FAILED       = 22;
    public static final int EVENT_PPPOE_DISCONNECT_SUCCESSED    = 23;
    public static final int EVENT_PHY_LINK_IN                   = 4;
    public static final int EVENT_PHY_LINK_OUT                  = 5;
    public static final int EVENT_INTERFACE_ADDED               = 6;
    public static final int EVENT_INTERFACE_REMOVED             = 7;
    public static final int EVENT_ETHERNET_ENABLED              = 8;
    public static final int EVENT_ETHERNET_DISABLED             = 9;

    public static final String EXTRA_NETWORK_INFO           = "networkInfo";
    public static final String EXTRA_LINK_PROPERTIES        = "linkProperties";
    public static final String EXTRA_ETHERNET_STATE         = "ethernet_state";

    IEthernetManager mService           = null;
    private Context mContext                    = null;
    private static EthernetManager mEthManager;

    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (msg.what == MSG_AVAILABILITY_CHANGED) {
                boolean isAvailable = (msg.arg1 == 1);
                for (Listener listener : mListeners) {
                    listener.onAvailabilityChanged(isAvailable);
                }
            }
        }
    };
    private final ArrayList<Listener> mListeners = new ArrayList<Listener>();
    private final IEthernetServiceListener.Stub mServiceListener =
            new IEthernetServiceListener.Stub() {
                @Override
                public void onAvailabilityChanged(boolean isAvailable) {
                    mHandler.obtainMessage(
                            MSG_AVAILABILITY_CHANGED, isAvailable ? 1 : 0, 0, null).sendToTarget();
                }
            };

    /**
     * Create a new EthernetManager instance.
     * you should get EthernetManager by EthernetManager.getInstance
     * Context.getSystemService() return com.net.EthernentManager
     */
    public static synchronized EthernetManager getInstance() {
        if (mEthManager == null)
            mEthManager = new EthernetManager();
        return mEthManager;
    }
 
    /**
     * A listener interface to receive notification on changes in Ethernet.
     */
    public interface Listener {
        /**
         * Called when Ethernet port's availability is changed.
         * @param isAvailable {@code true} if one or more Ethernet port exists.
         */
        public void onAvailabilityChanged(boolean isAvailable);
    }

    /**
     * Create a new EthernetManager instance.
     * Applications will almost always want to use
     * {@link android.content.Context#getSystemService Context.getSystemService()} to retrieve
     * the standard {@link android.content.Context#ETHERNET_SERVICE Context.ETHERNET_SERVICE}.
     */
    /**
     * @hide
     */
    private EthernetManager() {
        IBinder b = ServiceManager.getService(Context.ETHERNET_SERVICE);
        if (mService == null)
            mService = IEthernetManager.Stub.asInterface(b);
    }
	
    public EthernetManager(Context context, IEthernetManager service) {
        mContext = context;
        mService = service;
    }


    /**
     * @hide
     * Get Ethernet configuration.
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    public IpConfiguration getConfiguration() {
        try {
            return mService.getConfiguration();
        } catch (NullPointerException | RemoteException e) {
        	  Log.e(TAG, "getConfiguration failed!");
            return new IpConfiguration();
        }
    }

   /**
      * @hide
     * Get Ethernet configuration by assign type(static or pppoe).
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    public IpConfiguration getConfigurationByIpAssignment(String type) {
        try {
            return mService.getConfigurationByIpAssignment(type);
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "getConfiguration failed!");
            return new IpConfiguration();
        }
    }

    /**
     * @hide
     * Get Ethernet static configuration.
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    public IpConfiguration getStaticConfiguration() {
        return getConfigurationByIpAssignment(ETHERNET_CONNECT_MODE_MANUAL);
    }

    /**
     * @hide
     * Get Ethernet pppoe configuration.
     * @return the Ethernet Configuration, contained in {@link IpConfiguration}.
     */
    public IpConfiguration getPppoeConfiguration() {
        return getConfigurationByIpAssignment(ETHERNET_CONNECT_MODE_PPPOE);
    }


    /**
     * @hide
     * Set Ethernet configuration.
     */
    public void setConfiguration(IpConfiguration config) {
        try {
            mService.setConfiguration(config);
        } catch (NullPointerException | RemoteException e) {
        	Log.e(TAG, "setConfiguration failed!");
        }
    }

    /**
     * Indicates whether the system currently has one or more
     * Ethernet interfaces.
     */
    public boolean isAvailable() {
        try {
            return mService.isAvailable();
        } catch (NullPointerException | RemoteException e) {
            return false;
        }
    }

    /**
     *
     * get all the ethernet device names
     * @return interface name list on success, {@code null} on failure
     * @hide
     */
    public List<EthernetDevInfo> getDeviceList() {
        List<EthernetDevInfo> devList = new ArrayList<EthernetDevInfo>();
        if(isAvailable()) {
            return null;
        } else {
            devList.add(getDevInfo());
        }
        return devList;
    }

    /**
     * Get the selected network interface info
     */
    public EthernetDevInfo getDevInfo() {
        try {
            EthernetDevInfo info = new EthernetDevInfo();
            if(getEthernetMode().equals(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL)) {
                info = getStaticConfig();
            } else {
                info = new EthernetDevInfo(mService.getLinkProperties());
            }
            String mIfname = mService.getActiveIface();
            info.setIfName(mIfname);
            NetworkInfo mNetworkInfo = mService.getNetworkInfo();
            if(mNetworkInfo != null) {
                String mHwAddr = mNetworkInfo.getExtraInfo();
                info.setHwaddr(mHwAddr);
            }
            return info;
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "getDevInfo failed!");
            return null;
        }
    }

    public String getEthernetMode() {
        IpConfiguration config = getConfiguration();
        IpAssignment mIpAssignment = config.getIpAssignment();
        if(mIpAssignment == IpAssignment.STATIC) {
            return ETHERNET_CONNECT_MODE_MANUAL;
        } else if(mIpAssignment == IpAssignment.PPPOE) {
            return ETHERNET_CONNECT_MODE_PPPOE;
        } else {
            return ETHERNET_CONNECT_MODE_DHCP;
        }
    }

    public void setEthernetMode(String type, EthernetDevInfo info) {
        IpConfiguration config = info.getIpConfiguration(type);
        setConfiguration(config);
    }

    /**
     * Enable or Disable a ethernet service
     * @param enable {@code true} to enable, {@code false} to disable
     */
    public void setEthernetEnabled(boolean enable) {
        try {
            mService.setEthernetEnabled(enable);
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "setEthernetEnabled failed!");
        }
    }

    /**
     * Get ethernet service state
     * @return the state of the ethernet service
     *      ETHERNET_STATE_DISABLED
     *      ETHERNET_STATE_ENABLED
     *      ETHERNET_STATE_UNKNOWN
     */
    public int getEthernetState() {
        try {
           return mService.getEthernetState();
        } catch (NullPointerException | RemoteException e) {
        	  Log.e(TAG, "getEthernetState failed!");
            return ETHERNET_STATE_UNKNOWN;
        }
    }

    /**
     * Get current ethernet link state
     * @return
     *      false:  link down
     *      true:   link up
     */
    public boolean getLinkState() {
        try {
            return mService.getLinkState();
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "getLinkState failed!");
            return false;
        }
    }

    /**
     * get ethernet cheat state
     * @return
     *     false: off
     *     true:  on
     */
    public boolean getWifiDisguiseState() {
       return false;
    }

    /**
     * set ethernet cheat on/off
     */
    public void setWifiDisguise(boolean enable) {
        /* do nothing */
    }

    /**
     * Return ethernet information about the current configuration, if any is active.
     * @return the Ethernet device information, contained in {@link EthernetDevInfo}.
     */
    public synchronized EthernetDevInfo getStaticConfig() {
    	EthernetDevInfo info = new EthernetDevInfo();
        IpConfiguration config = getStaticConfiguration();
        StaticIpConfiguration staticConfig = config.getStaticIpConfiguration();
        
        try {
            String mIfname = mService.getActiveIface();
            info.setIfName(mIfname);
            NetworkInfo mNetworkInfo = mService.getNetworkInfo();
            if(mNetworkInfo != null) {
                String mHwAddr = mNetworkInfo.getExtraInfo();
                info.setHwaddr(mHwAddr);
            }
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "get hardware address or ifname failed");
        }

        if(staticConfig != null) {
            if(staticConfig.ipAddress != null) {
                info.setIpAddress(staticConfig.ipAddress.getAddress().getHostAddress());
                int prefixLength = 0xffffffff;
                prefixLength = prefixLength << (32 - staticConfig.ipAddress.getPrefixLength());
                prefixLength = Integer.reverseBytes(prefixLength);
                info.setNetMask(NetworkUtils.intToInetAddress(prefixLength).getHostAddress());
            }
            if(staticConfig.gateway != null) {
                info.setGateWay(staticConfig.gateway.getHostAddress());
            }
            int i = 1;
            for (InetAddress dns : staticConfig.dnsServers) {
                if(1 == i) {
                    info.setDns1(dns.getHostAddress());
                } else if(2 == i) {
                    info.setDns2(dns.getHostAddress());
                } else {
                    break;
                }
                i++;
            }
            info.setMode(ETHERNET_CONNECT_MODE_MANUAL);
        }
        return info;
    }

    /**
     * get usename and password in IPoE or PPPoE mode
     * @param mode
     *     ETHERNET_CONNECT_MODE_DHCP
     *     ETHERNET_CONNECT_MODE_PPPOE
     */
    public EthernetDevInfo getLoginInfo(String mode) {
    	EthernetDevInfo info = new EthernetDevInfo();
        IpConfiguration config = getPppoeConfiguration();
        if(mode.equals(ETHERNET_CONNECT_MODE_PPPOE)) {
            PppoeConfiguration pppoeConfig = config.getPppoeConfiguration();
            if(pppoeConfig != null) {
                info.setUsername(pppoeConfig.username);
                info.setPasswd(pppoeConfig.password);
            }
        }
        return info;
    }

    /*************************************
     ***     coordinate ali yunos      ***
     ************************************/
    /*@hide*/
    public static final String EXTRA_ETHERNET_INFO		= "ethernetInfo";
    /*@hide*/
    public static final int EVENT_DEVREM = EVENT_INTERFACE_ADDED;
    /*@hide*/
    public static final int EVENT_NEWDEV	= EVENT_INTERFACE_REMOVED;
    /*@hide*/
    public static final int EVENT_CONFIGURATION_SUCCEEDED = EVENT_ETHERNET_CONNECT_SUCCESSED;
    /*@hide*/
    public static final int EVENT_CONFIGURATION_FAILED = EVENT_ETHERNET_CONNECT_FAILED;
    /*@hide*/
    public static final int EVENT_DISCONNECTED = EVENT_ETHERNET_DISCONNECT_SUCCESSED;
    /*@hide*/
    public static final int ETHERNET_INTERFACT_ADDED = EVENT_INTERFACE_ADDED;
    /*@hide*/
    public static final int ETHERNET_INTERFACT_REMOVED = EVENT_INTERFACE_REMOVED;

    /*@hide*/
    public static final String ETHERNET_LINKED_ACTION = ETHERNET_STATE_CHANGED_ACTION;
    /**@hide*/
    public static final String ETHERNET_DISLINKED_ACTION = ETHERNET_STATE_CHANGED_ACTION;
    /*@hide*/
    public static final String PPPOE_STATE_CHANGED_ACTION = ETHERNET_STATE_CHANGED_ACTION;
    /*@hide*/
    public static final String EXTRA_PPPOE_STATE = EXTRA_ETHERNET_STATE;
    /*@hide*/
    public static final int PPPOE_STATE_ENABLED = ETHERNET_STATE_ENABLED;
    /*@hide*/
    public static final int PPPOE_STATE_DISABLED = ETHERNET_STATE_DISABLED;
    /*@hide*/
    public static final String ETHERNET_INTERFACE_CHANGED_ACTION = ETHERNET_STATE_CHANGED_ACTION;

    /* @hide */
    public boolean isConfigured() {
        if(getDevInfo() == null)
            return false;
        else
            return true;
    }

    /**
     * @hide - it is same with getStaticConfig()
     */
    public EthernetDevInfo getSavedConfig() {
        return getStaticConfig();
    }


    /**
     * return DhcpInfo if needed, return EthernetDevInfo use getDevInfo
     */
    public DhcpInfo getDhcpInfo() {
        try {
            return mService.getDhcpInfo();
        } catch (NullPointerException | RemoteException e) {
            Log.e(TAG, "getDhcpInfo failed!");
            return null;
        }
    }

    /**
     * @hide - it is same with setStaticConfig
     */
    public void updateDevInfo(EthernetDevInfo info) {
        String mode = info.getMode();
        setEthernetMode(mode, info);
    }

     /**
     * @hide - it is same with getDeviceList
     */
    public List<EthernetDevInfo> getDeviceNameList() {
        return getDeviceList();
    }

    /**
     * @hide - it is same with setEthernetEnabled
     */
    public void setEnabled(boolean enable) {
        setEthernetEnabled(enable);
    }

    /**
     * @hide - it is same with getEthernetState
     */
    public int getState() {
        return getEthernetState();
    }

    /**
     * @hide
     * @return
     *     true  : ethernet enabled
     *     false : ethernet disabled
     */
    public boolean isOn() {
        return getEthernetState() == ETHERNET_STATE_ENABLED;
    }

    /* @hide */
    public boolean isDhcp() {
        return getEthernetMode().equals(ETHERNET_CONNECT_MODE_DHCP);
    }

    /* @hide */
    public void setEthIfExist(boolean ifExist) {
        /*
         * do nothing
         */
        return;
    }

    /* @hide */
    public boolean isEthIfExist() {
        /*
         * because we build ethernet driver into kernel
         * so return true
         */
        return true;
    }

    /**
     * @param ifname the string that identifies the network interface
     * check if the interface linkin or not.
     * cat sys/class/net/ethx/carrier 1 means link in,
     * carrier is 0 or doesn't exist means link out.
     * @return {@code true} if linkin
     */
    public int checkLink(String ifname) {
        try {
            return mService.checkLink(ifname)? 1 : 0;
        } catch (RemoteException e) {
            return 0;
        }
    }

    /*******************************************
     ***       END(coordinate ali yunos)     ***
     ******************************************/

/*----------------------------------------------------------*/

    /**********************************************
     ***          coordinate CMCC-WASU          ***
     *********************************************/

    /* @hide */
    public static final int ETHERNET_DEVICE_SCAN_RESULT_READY = 0;

    /* @hide */
    public static final int EVENT_DHCP_CONNECT_SUCCESSED = 10;
    /* @hide */
    public static final int EVENT_DHCP_CONNECT_FAILED = 11;
    /* @hide */
    public static final int EVENT_DHCP_DISCONNECT_SUCCESSED = 12;
    /* @hide */
    public static final int EVENT_DHCP_DISCONNECT_FAILED = 13;

    /* @hide */
    public static final int EVENT_STATIC_CONNECT_SUCCESSED = 14;
    /* @hide */
    public static final int EVENT_STATIC_CONNECT_FAILED = 15;
    /* @hide */
    public static final int EVENT_STATIC_DISCONNECT_SUCCESSED = 16;
    /* @hide */
    public static final int EVENT_STATIC_DISCONNECT_FAILED = 17;
    /* @hide */
    public static final int EVENT_PHY_LINK_UP =18;
    /* @hide */
    public static final int EVENT_PHY_LINK_DOWN = 19;

    /**
     * @hide - set dhcp mode as default config
     */
    public void setDefaultConf() {
        DhcpInfo dhcpInfo = new DhcpInfo();
        setEthernetMode(ETHERNET_CONNECT_MODE_DHCP, dhcpInfo);
    }

    /**
     * @hide - it is same with checkLink
     */
    public int CheckLink(String ifname) {
        try {
            return mService.checkLink(ifname)? 1 : 0;
        } catch (RemoteException e) {
            return 0;
        }
    }

    /**
     * @hide - it is same with isConfigured
     */
    public boolean isEthernetConfigured() {
        return isConfigured();
    }

    /**
     * set ethernet config
     * @param mode
     *     ETHERNET_CONNECT_MODE_DHCP
     *     ETHERNET_CONNECT_MODE_MANUAL
     * @hide
     */
    public void setEthernetMode(String mode, DhcpInfo dhcpInfo) {
        String ipAddr = addrToString(dhcpInfo.ipAddress);
        String gwAddr = addrToString(dhcpInfo.gateway);
        String maskAddr = addrToString(dhcpInfo.netmask);
        String dns1Addr = addrToString(dhcpInfo.dns1);
        String dns2Addr = addrToString(dhcpInfo.dns2);

        EthernetDevInfo cfg = getSavedConfig();
        cfg.setMode(ETHERNET_CONNECT_MODE_MANUAL);
        cfg.setIpAddress(ipAddr);
        cfg.setGateWay(gwAddr);
        cfg.setNetMask(maskAddr);
        cfg.setDns1(dns1Addr);
        cfg.setDns2(dns2Addr);
        setEthernetMode(mode, cfg);
    }

    /* @hide */
    public String getInterfaceName() {
        Log.d(TAG, " getInterfaceName:");
        return getDevInfo().getIfName();
    }

    /* @hide */
    public boolean setInterfaceName(String ifName) {
        /*
         * do nothing
        */
        return true;
    }

	  /* @hide */
    public boolean getNetLinkStatus() {
        return getNetLinkStatus(getInterfaceName()) > 0;
    }
    
    /**
     * @hide
     * @param ifaceName
     * @return
     */
    public int getNetLinkStatus(String ifaceName) {
        return CheckLink(ifaceName);
    }

    static String addrToString( int addr ) {
        int v1 = addr&0xff,v2=(addr>>8)&0xff,v3=(addr>>16)&0xff,v4=(addr>>24)&0xff;
        return ""+v1+"."+v2+"."+v3+"."+v4;
    }

    static int parseInetAddr(String addr ) {
        if ( addr == null || !addr.matches("^\\d+(\\.\\d+){3}$") ) {
            return 0;
        }
        int ipaddr = 0;
        int pos = 0;
        for( String v: addr.split("\\.") ) {
            int val = Integer.parseInt(v)&0xFF;
            ipaddr = (ipaddr)|(val<<(8*pos));
            ++pos;
        }
        return ipaddr;
    }

    /**********************************************
     ***        END(coordinate CMCC-WASU)       ***
     *********************************************/
	 
	 
	    /**
     * Adds a listener.
     * @param listener A {@link Listener} to add.
     * @throws IllegalArgumentException If the listener is null.
     */
    public void addListener(Listener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("listener must not be null");
        }
        mListeners.add(listener);
        if (mListeners.size() == 1) {
            try {
                mService.addListener(mServiceListener);
            } catch (NullPointerException | RemoteException e) {
            }
        }
    }

    /**
     * Removes a listener.
     * @param listener A {@link Listener} to remove.
     * @throws IllegalArgumentException If the listener is null.
     */
    public void removeListener(Listener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("listener must not be null");
        }
        mListeners.remove(listener);
        if (mListeners.isEmpty()) {
            try {
                mService.removeListener(mServiceListener);
            } catch (NullPointerException | RemoteException e) {
            }
        }
    }

}
