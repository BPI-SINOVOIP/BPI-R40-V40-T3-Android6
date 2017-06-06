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

package android.net;

import android.net.LinkAddress;
import android.os.Parcelable;
import android.os.Parcel;
import android.util.Log;

import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Class that describes pppoe configuration.
 *
 * This class is different from LinkProperties because it represents
 * configuration intent. The general contract is that if we can represent
 * a configuration here, then we should be able to configure it on a network.
 * The intent is that it closely match the UI we have for configuring networks.
 *
 * In contrast, LinkProperties represents current state. It is much more
 * expressive. For example, it supports multiple IP addresses, multiple routes,
 * stacked interfaces, and so on. Because LinkProperties is so expressive,
 * using it to represent configuration intent as well as current state causes
 * problems. For example, we could unknowingly save a configuration that we are
 * not in fact capable of applying, or we could save a configuration that the
 * UI cannot display, which has the potential for malicious code to hide
 * hostile or unexpected configuration from the user: see, for example,
 * http://b/12663469 and http://b/16893413 .
 *
 * @hide
 */
public class PppoeConfiguration implements Parcelable {
    private static final String TAG                 = "PppoeConfiguration";
    private final String PPPOE_PAP_CONFIG_FILE      = "/data/system/pap-secrets";
    private final String PPPOE_CHAP_CONFIG_FILE     = "/data/system/chap-secrets";
    private final String PPPOE_CONFIG_FORMAT        = "\"%s\" * \"%s\"";

    public String username;
    public String password;

    public PppoeConfiguration() {
        username = null;
        password = null;
    }

    public PppoeConfiguration(PppoeConfiguration source) {
        this();
        if (source != null) {
            // All of these except dnsServers are immutable, so no need to make copies.
            username = source.username;
            password = source.password;
        }
    }

    public void clear() {
        username = null;
        password = null;
    }

    public void saveLoginInfo(String intf) {
        String pap_file_path;
        String chap_file_path;
        if(intf == null || intf.length() == 0) {
            pap_file_path = PPPOE_PAP_CONFIG_FILE;
            chap_file_path = PPPOE_CHAP_CONFIG_FILE;
        } else {
            pap_file_path = String.format("%s-%s", PPPOE_PAP_CONFIG_FILE, intf);
            chap_file_path = String.format("%s-%s", PPPOE_CHAP_CONFIG_FILE, intf);
        }
        File pap_file = new File(pap_file_path);
        File chap_file = new File(chap_file_path);
        String loginInfo = String.format(PPPOE_CONFIG_FORMAT, username, password);
        try {
            // 1. Save pap-secrets
            BufferedOutputStream out = new BufferedOutputStream(
                    new FileOutputStream(pap_file));
            out.write(loginInfo.getBytes(), 0, loginInfo.length());
            Log.d(TAG, "write to " + PPPOE_PAP_CONFIG_FILE
                    + " login info = " + loginInfo);
            out.flush();
            out.close();
            // 2. Save chap-secrets
            out = new BufferedOutputStream(
                    new FileOutputStream(chap_file));
            out.write(loginInfo.getBytes(), 0, loginInfo.length());
            Log.d(TAG, "write to " + PPPOE_CHAP_CONFIG_FILE
                    + " login info = " + loginInfo);
            out.flush();
            out.close();
        } catch (IOException e) {
            Log.e(TAG, "Write PPPoE config failed!" + e);
        }
    }

    public String toString() {
        StringBuffer str = new StringBuffer();

        str.append("username ");
        if (username != null ) str.append(username).append(" ");

        str.append("password ");
        if (password != null) str.append(password);

        return str.toString();
    }

    public int hashCode() {
        int result = 13;
        result = 47 * result + (username == null ? 0 : username.hashCode());
        result = 47 * result + (password == null ? 0 : password.hashCode());
        return result;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;

        if (!(obj instanceof PppoeConfiguration)) return false;

        PppoeConfiguration other = (PppoeConfiguration) obj;

        return other != null &&
                Objects.equals(username, other.username) &&
                Objects.equals(password, other.password);
    }

    /** Implement the Parcelable interface */
    public static Creator<PppoeConfiguration> CREATOR =
        new Creator<PppoeConfiguration>() {
            public PppoeConfiguration createFromParcel(Parcel in) {
                PppoeConfiguration s = new PppoeConfiguration();
                readFromParcel(s, in);
                return s;
            }

            public PppoeConfiguration[] newArray(int size) {
                return new PppoeConfiguration[size];
            }
        };

    /** Implement the Parcelable interface */
    public int describeContents() {
        return 0;
    }

    /** Implement the Parcelable interface */
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(username);
        dest.writeString(password);
    }

    protected static void readFromParcel(PppoeConfiguration s, Parcel in) {
        s.username = in.readString();
        s.password = in.readString();
    }
}