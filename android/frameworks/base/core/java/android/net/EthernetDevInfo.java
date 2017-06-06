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

package android.net;

import android.os.Parcel;
import android.os.Parcelable;
import android.os.Parcelable.Creator;
import android.net.EthernetManager;
import android.net.IpConfiguration;
import android.net.IpConfiguration.ProxySettings;
import android.net.IpConfiguration.IpAssignment;
import android.net.StaticIpConfiguration;
import android.net.PppoeConfiguration;
import android.net.NetworkUtils;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.RouteInfo;

import java.net.InetAddress;
import java.net.Inet4Address;
import android.util.Log;
/**
 * Describes the state of Ethernet connection that is active or
 * is in the process of being set up.
 */
public class EthernetDevInfo implements Parcelable {
    /**
     * The ethernet interface is configured by dhcp
     * @hide - add this for coordinate CMCC-WASU
     */
    public static final int ETHERNET_CONN_MODE_DHCP = 1;
    /**
     * The ethernet interface is configured manually
     * @hide - add this for coordinate CMCC-WASU
     */
    public static final int ETHERNET_CONN_MODE_MANUAL = 0;

    private String ifacename;
    private String hwaddr;
    private String ipaddr;
    private String gateway;
    private String netmask;
    private String dns1;
    private String dns2;
    private String mode;
    private String passwd;
    private String username;
    private String route;
    private int proxy_on;
    private String proxy_host;
    private String proxy_port;
    private IpConfiguration mIpConfiguration;
    public EthernetDevInfo () {
        ifacename = null;
        hwaddr = null;
        ipaddr = null;
        gateway = null;
        netmask = null;
        dns1 = null;
        dns2 = null;
        mode = EthernetManager.ETHERNET_CONNECT_MODE_DHCP;
        username = null;
        passwd = null;
        proxy_on = 0;
        proxy_host = null;
        proxy_port = null;
        mIpConfiguration = new IpConfiguration();
    }
    
    public EthernetDevInfo (LinkProperties lp) {
        this();
        // Convert LinkProperties to EthernetDevInfo
        if (lp == null) {
            return;
        }
        for (LinkAddress i : lp.getLinkAddresses()) {
            // Set IP
            ipaddr = i.getAddress().getHostAddress();
            // Set netmask(0xffffff00 ---> 255.255.255.0)
            // Attent network byte order
            int r_mask = 0xffffffff;
            Log.e("ethernet", "mask = " + i.getPrefixLength());
            r_mask = r_mask << (32 - i.getPrefixLength());
            r_mask = Integer.reverseBytes(r_mask);
            netmask = NetworkUtils.intToInetAddress(r_mask).getHostAddress();
            break;
        }
        for (RouteInfo i : lp.getRoutes()) {
            if (i.isDefaultRoute()) {
                gateway = i.getGateway().getHostAddress();
            }
        }
        int j = 0;
        for (InetAddress i : lp.getDnsServers()) {
            if (j == 0) {
                dns1 = i.getHostAddress();
            } else if (j == 1) {
                dns2 = i.getHostAddress();
            } else {
                break;
            }
            j++;
        }
    }
    /**
     * save interface name into the configuration
     */
    public void setIfName(String ifname) {
        this.ifacename = ifname;
    }

    /**
     * Returns the interface name from the saved configuration
     * @return interface name
     */
    public String getIfName() {
        return this.ifacename;
    }

    /**
     * Save Mac address
     */
    public void setHwaddr(String hwaddr) {
        this.hwaddr = hwaddr;
    }

    /**
     * Return the MAC address of the network interface
     * @return
     */
    public String getHwaddr( ) {
        return this.hwaddr;
    }

    /*
     * Get device MacAddress
     * @hide - used by CMCC-WASU only, normally you should use "getHwaddr"
     */
    public String getMacAddress(){
        return getHwaddr();
    }

    /**
     * Save IP address
     */
    public void setIpAddress(String ip) {
        this.ipaddr = ip;
    }

    /**
     * Return the IP address of the network interface
     */
    public String getIpAddress( ) {
        return this.ipaddr;
    }

    /**
     * Save gateway
     */
    public void setGateWay(String gw) {
        this.gateway = gw;
    }

    /**
     * Return the gateway of the network interface
     */
    public String getGateWay() {
        return this.gateway;
    }

    /**
     * Save netmask
     */
    public void setNetMask(String ip) {
        this.netmask = ip;
    }

    /**
     * Return the netmask of the network interface
     */
    public String getNetMask( ) {
        return this.netmask;
    }

    /**
     * Save dns1
     */
    public void setDns1(String dns) {
        this.dns1 = dns;
    }

    /**
     * Return the dns1 of the network interface
     */
    public String getDns1( ) {
        return this.dns1;
    }

    /**
     * Save dns1
     * @hide - used by CMCC_WASU only, normally you should use "setDns1"
     */
    public void setDnsAddr(String dns) {
        setDns1(dns);
    }

    /**
     * Return the dns1 of this network interface
     * @hide - used by CMCC-WASU only, normally you should use "getDns1"
     */
    public String getDnsAddr( ) {
        return getDns1();
    }
    /**
     * Save dns2
     */
    public void setDns2(String dns) {
        this.dns2 = dns;
    }

    /**
     * Return the dns2 of the network interface
     */
    public String getDns2( ) {
        return this.dns2;
    }

    /**
     * Save dns2
     * @hide - used by CMCC_WASU only, normally you should use "setDns2"
     */
    public void setDns2Addr(String dns2) {
        setDns2(dns2);
    }

    /**
     * Return the dns2 of this network interface
     * @hide - used by CMCC-WASU only, normally you should use "getDns2"
     */
    public String getDns2Addr() {
        return getDns2();
    }

    /**
     * Set ethernet connect mode of the network interface
     * @param mode
     * @return
     */
    public void setMode(String mode) {
        this.mode = mode;
    }

    /**
     * Return ethernet connect mode
     */
    public String getMode() {
        return this.mode;
    }

    /* set ethernet connect mode of the network interface
     * @hide - used by aliyun os only, normally you should use "setMode"
     */
    public void setConnectMode(int mode) {
        if(mode == ETHERNET_CONN_MODE_DHCP) {
            setMode(EthernetManager.ETHERNET_CONNECT_MODE_DHCP);
        } else if (mode == ETHERNET_CONN_MODE_MANUAL) {
            setMode(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL);
        }
    }

    /* return ethernet connect mode
     * @hide - used by CMCC-WASU only, normally you should use "getMode"
     */
    public int getConnectMode() {
        String mode = getMode();
        if(mode.equals(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL)) {
            return ETHERNET_CONN_MODE_MANUAL;
        } else {
            return ETHERNET_CONN_MODE_DHCP;
        }
    }
    /**
     * Set the passwd that the current connect needed
     */
    public void setPasswd(String pw) {
        this.passwd = pw;
    }

    /**
     * Return the passwd
     */
    public String getPasswd() {
        return this.passwd;
    }

    /**
     * Set the username that the current connect mode needed
     */
    public void setUsername(String username) {
        this.username = username;
    }

    /**
     * Return username
     */
    public String getUsername() {
        return this.username;
    }

    /** @hide */
    public IpConfiguration getIpConfiguration(String type) {
        IpConfiguration mIpConfiguration = new IpConfiguration(IpAssignment.DHCP,
                ProxySettings.NONE, null, null,null);
        if(type.equals(EthernetManager.ETHERNET_CONNECT_MODE_DHCP)) {
            mIpConfiguration.setIpAssignment(IpAssignment.DHCP);
        } else if(type.equals(EthernetManager.ETHERNET_CONNECT_MODE_MANUAL)) {
            mIpConfiguration.setIpAssignment(IpAssignment.STATIC);
            StaticIpConfiguration staticConfig = new StaticIpConfiguration();
            mIpConfiguration.setStaticIpConfiguration(staticConfig);
            if(this.ipaddr != null && this.ipaddr.length() > 0) {
                Inet4Address inetAddr = getIPv4Address(this.ipaddr);
                int prefixLength = -1;
                if(this.netmask != null && this.netmask.length() > 0) {
                    InetAddress mask = NetworkUtils.numericToInetAddress(this.netmask);
                    prefixLength = NetworkUtils.netmaskIntToPrefixLength(NetworkUtils.inetAddressToInt((Inet4Address)mask));
                }
                if(prefixLength < 0 || prefixLength > 32) {
                    prefixLength = 0;
                }
                staticConfig.ipAddress = new LinkAddress(inetAddr, prefixLength);
            }
            if(this.gateway != null && this.gateway.length() > 0) {
                staticConfig.gateway = getIPv4Address(this.gateway);
            }
            if(this.dns1 != null && this.dns1.length() > 0) {
                staticConfig.dnsServers.add(getIPv4Address(this.dns1));
            }
            if(this.dns2 != null && this.dns2.length() > 0) {
                staticConfig.dnsServers.add(getIPv4Address(this.dns2));
            }
        } else if(type.equals(EthernetManager.ETHERNET_CONNECT_MODE_PPPOE)) {
            mIpConfiguration.setIpAssignment(IpAssignment.PPPOE);
            PppoeConfiguration pppoeConfig = new PppoeConfiguration();
            mIpConfiguration.setPppoeConfiguration(pppoeConfig);
            pppoeConfig.username = this.username;
            pppoeConfig.password = this.passwd;
        }
        return mIpConfiguration;
    }
    
    private Inet4Address getIPv4Address(String text) {
        try {
            return (Inet4Address) NetworkUtils.numericToInetAddress(text);
        } catch (IllegalArgumentException|ClassCastException e) {
            return null;
        }
    }

    public int describeContents() {
        return 0;
    }

    /**
     * @hide - used by CMCC-WASU only, normally you should not use this method
     */
    public void setRouteAddr(String route) {
        //Slog.i(TAG, "setRouteAddr:"+route);
        if (route!=null)
            this.route = route;
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public String getRouteAddr() {
        return this.route;
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public void setProxyOn(boolean enabled) {
        this.proxy_on = (enabled ? 1 : 0);
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public boolean getProxyOn() {
        return ( (this.proxy_on==1) ? true : false );
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public void setProxyHost(String host) {
        //Slog.i(TAG, "setProxyHost:"+host);
        if ( host != null ) {
            this.proxy_host = host;
        }
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public String getProxyHost( ) {
        return this.proxy_host;
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public void setProxyPort(String port) {
        //Slog.i(TAG, "setProxyPort:"+port);
        if ( port != null ) {
            this.proxy_port = port;
        }
    }

    /**
     * @hide - used by CMCC-WASU only, normally you don't need this method
     */
    public String getProxyPort( ) {
        return this.proxy_port;
    }

    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(this.ifacename);
        dest.writeString(this.hwaddr);
        dest.writeString(this.ipaddr);
        dest.writeString(this.gateway);
        dest.writeString(this.netmask);
        dest.writeString(this.dns1);
        dest.writeString(this.dns2);
        dest.writeString(this.mode);
        dest.writeString(this.passwd);
        dest.writeString(this.username);
    }

    /** Implement the Parcelable interface {@hide} */
    public static final Creator<EthernetDevInfo> CREATOR = new Creator<EthernetDevInfo>() {
        public EthernetDevInfo createFromParcel(Parcel in) {
            EthernetDevInfo info = new EthernetDevInfo();
            info.setIfName(in.readString());
            info.setHwaddr(in.readString());
            info.setIpAddress(in.readString());
            info.setGateWay(in.readString());
            info.setNetMask(in.readString());
            info.setDns1(in.readString());
            info.setDns2(in.readString());
            info.setMode(in.readString());
            info.setPasswd(in.readString());
            info.setUsername(in.readString());
            info.setRouteAddr(in.readString());
            info.setProxyOn((in.readInt()==1)?true:false);
            info.setProxyHost(in.readString());
            info.setProxyPort(in.readString());
            return info;
        }

        public EthernetDevInfo[] newArray(int size) {
            return new EthernetDevInfo[size];
        }
    };
}
