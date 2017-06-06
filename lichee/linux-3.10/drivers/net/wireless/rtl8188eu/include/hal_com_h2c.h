/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __COMMON_H2C_H__
#define __COMMON_H2C_H__

//---------------------------------------------------------------------------------------------------------//
//----------------------------------    H2C CMD DEFINITION    ------------------------------------------------//
//---------------------------------------------------------------------------------------------------------//
// 88e, 8723b, 8812, 8821, 92e use the same FW code base

#define H2C_RSVDPAGE_LOC_LEN		5
#define H2C_MEDIA_STATUS_RPT_LEN		3
#define H2C_KEEP_ALIVE_CTRL_LEN	2
#define H2C_DISCON_DECISION_LEN	 	3
#define H2C_AP_OFFLOAD_LEN		3
#define H2C_AP_WOW_GPIO_CTRL_LEN		3
#define H2C_PWRMODE_LEN			7
#define H2C_PSTUNEPARAM_LEN			4
#define H2C_MACID_CFG_LEN	 	7
#define H2C_BTMP_OPER_LEN			4
#define H2C_WOWLAN_LEN			4
#define H2C_REMOTE_WAKE_CTRL_LEN	3
#define H2C_AOAC_GLOBAL_INFO_LEN	2
#define H2C_AOAC_RSVDPAGE_LOC_LEN	7
#define H2C_SCAN_OFFLOAD_CTRL_LEN	4
#define H2C_BT_FW_PATCH_LEN			6
#define H2C_RSSI_SETTING_LEN		4
#define H2C_AP_REQ_TXRPT_LEN		2
#define H2C_FORCE_BT_TXPWR_LEN		3
#define H2C_BCN_RSVDPAGE_LEN		5
#define H2C_PROBERSP_RSVDPAGE_LEN	5

#ifdef CONFIG_WOWLAN	
#define eqMacAddr(a,b)						( ((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3] && (a)[4]==(b)[4] && (a)[5]==(b)[5]) ? 1:0 )
#define cpMacAddr(des,src)					((des)[0]=(src)[0],(des)[1]=(src)[1],(des)[2]=(src)[2],(des)[3]=(src)[3],(des)[4]=(src)[4],(des)[5]=(src)[5])
#define cpIpAddr(des,src)					((des)[0]=(src)[0],(des)[1]=(src)[1],(des)[2]=(src)[2],(des)[3]=(src)[3])

//
// ARP packet
//
// LLC Header
#define GET_ARP_PKT_LLC_TYPE(__pHeader) 					ReadEF2Byte( ((u8*)(__pHeader)) + 6)

//ARP element
#define GET_ARP_PKT_OPERATION(__pHeader) 				ReadEF2Byte(((u8*)(__pHeader)) + 6)
#define GET_ARP_PKT_SENDER_MAC_ADDR(__pHeader, _val) 	cpMacAddr((u8*)(_val), ((u8*)(__pHeader))+8)
#define GET_ARP_PKT_SENDER_IP_ADDR(__pHeader, _val) 		cpIpAddr((u8*)(_val), ((u8*)(__pHeader))+14)
#define GET_ARP_PKT_TARGET_MAC_ADDR(__pHeader, _val) 	cpMacAddr((u8*)(_val), ((u8*)(__pHeader))+18)
#define GET_ARP_PKT_TARGET_IP_ADDR(__pHeader, _val) 	cpIpAddr((u8*)(_val), ((u8*)(__pHeader))+24)

#define SET_ARP_PKT_HW(__pHeader, __Value)  				WriteEF2Byte( ((u8*)(__pHeader)) + 0, __Value)
#define SET_ARP_PKT_PROTOCOL(__pHeader, __Value)  			WriteEF2Byte( ((u8*)(__pHeader)) + 2, __Value)
#define SET_ARP_PKT_HW_ADDR_LEN(__pHeader, __Value)  		WriteEF1Byte( ((u8*)(__pHeader)) + 4, __Value)
#define SET_ARP_PKT_PROTOCOL_ADDR_LEN(__pHeader, __Value)  	WriteEF1Byte( ((u8*)(__pHeader)) + 5, __Value)
#define SET_ARP_PKT_OPERATION(__pHeader, __Value) 		WriteEF2Byte( ((u8*)(__pHeader)) + 6, __Value)
#define SET_ARP_PKT_SENDER_MAC_ADDR(__pHeader, _val) 	cpMacAddr(((u8*)(__pHeader))+8, (u8*)(_val))
#define SET_ARP_PKT_SENDER_IP_ADDR(__pHeader, _val) 		cpIpAddr(((u8*)(__pHeader))+14, (u8*)(_val))
#define SET_ARP_PKT_TARGET_MAC_ADDR(__pHeader, _val) 	cpMacAddr(((u8*)(__pHeader))+18, (u8*)(_val))
#define SET_ARP_PKT_TARGET_IP_ADDR(__pHeader, _val) 		cpIpAddr(((u8*)(__pHeader))+24, (u8*)(_val))

#define FW_WOWLAN_FUN_EN				BIT(0)
#define FW_WOWLAN_PATTERN_MATCH			BIT(1)
#define FW_WOWLAN_MAGIC_PKT				BIT(2)
#define FW_WOWLAN_UNICAST				BIT(3)
#define FW_WOWLAN_ALL_PKT_DROP			BIT(4)
#define FW_WOWLAN_GPIO_ACTIVE			BIT(5)
#define FW_WOWLAN_REKEY_WAKEUP			BIT(6)
#define FW_WOWLAN_DEAUTH_WAKEUP			BIT(7)

#define FW_WOWLAN_GPIO_WAKEUP_EN		BIT(0)
#define FW_FW_PARSE_MAGIC_PKT			BIT(1)

#define FW_REMOTE_WAKE_CTRL_EN			BIT(0)
#define FW_REALWOWLAN_EN				BIT(5)

#define FW_WOWLAN_KEEP_ALIVE_EN			BIT(0)
#define FW_ADOPT_USER					BIT(1)
#define FW_WOWLAN_KEEP_ALIVE_PKT_TYPE	BIT(2)

#define FW_REMOTE_WAKE_CTRL_EN			BIT(0)
#define FW_ARP_EN						BIT(1)
#define FW_REALWOWLAN_EN				BIT(5)
#define FW_WOW_FW_UNICAST_EN			BIT(7)

#endif //CONFIG_WOWLAN

// _WoWLAN PARAM_CMD_0x80
#define SET_H2CCMD_WOWLAN_FUNC_ENABLE(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 0, 1, __Value)
#define SET_H2CCMD_WOWLAN_PATTERN_MATCH_ENABLE(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 1, 1, __Value)
#define SET_H2CCMD_WOWLAN_MAGIC_PKT_ENABLE(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 2, 1, __Value)
#define SET_H2CCMD_WOWLAN_UNICAST_PKT_ENABLE(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 3, 1, __Value)
#define SET_H2CCMD_WOWLAN_ALL_PKT_DROP(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 4, 1, __Value)
#define SET_H2CCMD_WOWLAN_GPIO_ACTIVE(__pH2CCmd, __Value)				SET_BITS_TO_LE_1BYTE(__pH2CCmd, 5, 1, __Value)
#define SET_H2CCMD_WOWLAN_REKEY_WAKE_UP(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 6, 1, __Value)
#define SET_H2CCMD_WOWLAN_DISCONNECT_WAKE_UP(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 7, 1, __Value)
#define SET_H2CCMD_WOWLAN_GPIONUM(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+1, 0, 7, __Value)
#define SET_H2CCMD_WOWLAN_DATAPIN_WAKE_UP(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+1, 7, 1, __Value)
#define SET_H2CCMD_WOWLAN_GPIO_DURATION(__pH2CCmd, __Value)			SET_BITS_TO_LE_1BYTE((__pH2CCmd)+2, 0, 8, __Value)
//#define SET_H2CCMD_WOWLAN_GPIO_PULSE_EN(__pH2CCmd, __Value)			SET_BITS_TO_LE_1BYTE((__pH2CCmd)+3, 0, 1, __Value)
#define SET_H2CCMD_WOWLAN_GPIO_PULSE_COUNT(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE((__pH2CCmd)+3, 0, 8, __Value)

// _REMOTE_WAKEUP_CMD_0x81
#define SET_H2CCMD_REMOTE_WAKECTRL_ENABLE(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 0, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_ARP_OFFLOAD_EN(__pH2CCmd, __Value)				SET_BITS_TO_LE_1BYTE(__pH2CCmd, 1, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_NDP_OFFLOAD_EN(__pH2CCmd, __Value)				SET_BITS_TO_LE_1BYTE(__pH2CCmd, 2, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_GTK_OFFLOAD_EN(__pH2CCmd, __Value)				SET_BITS_TO_LE_1BYTE(__pH2CCmd, 3, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_NLO_OFFLOAD_EN(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE(__pH2CCmd, 4, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_FW_UNICAST_EN(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE(__pH2CCmd, 7, 1, __Value)
#define SET_H2CCMD_REMOTE_WAKE_CTRL_ARP_ACTION(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE((__pH2CCmd)+2, 0, 1, __Value)

// AOAC_GLOBAL_INFO_0x82
#define SET_H2CCMD_AOAC_GLOBAL_INFO_PAIRWISE_ENC_ALG(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE(__pH2CCmd, 0, 8, __Value)
#define SET_H2CCMD_AOAC_GLOBAL_INFO_GROUP_ENC_ALG(__pH2CCmd, __Value)			SET_BITS_TO_LE_1BYTE((__pH2CCmd)+1, 0, 8, __Value)

// AOAC_RSVDPAGE_LOC_0x83
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_REMOTE_WAKE_CTRL_INFO(__pH2CCmd, __Value)		SET_BITS_TO_LE_1BYTE((__pH2CCmd), 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_ARP_RSP(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+1, 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_NEIGHBOR_ADV(__pH2CCmd, __Value)			SET_BITS_TO_LE_1BYTE((__pH2CCmd)+2, 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_RSP(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+3, 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_INFO(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+4, 0, 8, __Value)
#ifdef CONFIG_GTK_OL
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_GTK_EXT_MEM(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+5, 0, 8, __Value)
#endif //CONFIG_GTK_OL
#ifdef CONFIG_PNO_SUPPORT
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_NLO_INFO(__pH2CCmd, __Value)					SET_BITS_TO_LE_1BYTE((__pH2CCmd)+6, 0, 8, __Value)
#endif

#ifdef CONFIG_PNO_SUPPORT
// D0_Scan_Offload_Info_0x86
#define SET_H2CCMD_AOAC_NLO_FUN_EN(__pH2CCmd, __Value)			SET_BITS_TO_LE_1BYTE((__pH2CCmd), 3, 1, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_PROBE_PACKET(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE((__pH2CCmd)+1, 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_SCAN_INFO(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE((__pH2CCmd)+2, 0, 8, __Value)
#define SET_H2CCMD_AOAC_RSVDPAGE_LOC_SSID_INFO(__pH2CCmd, __Value)	SET_BITS_TO_LE_1BYTE((__pH2CCmd)+3, 0, 8, __Value)
#endif //CONFIG_PNO_SUPPORT

//---------------------------------------------------------------------------------------------------------//
//-------------------------------------------    Structure    --------------------------------------------------//
//---------------------------------------------------------------------------------------------------------//
typedef struct _RSVDPAGE_LOC {
	u8 LocProbeRsp;
	u8 LocPsPoll;
	u8 LocNullData;
	u8 LocQosNull;
	u8 LocBTQosNull;
#ifdef CONFIG_WOWLAN
	u8 LocRemoteCtrlInfo;
	u8 LocArpRsp;
	u8 LocNbrAdv;
	u8 LocGTKRsp;
	u8 LocGTKInfo;
	u8 LocProbeReq;
	u8 LocNetList;
#ifdef CONFIG_GTK_OL
	u8 LocGTKEXTMEM;
#endif //CONFIG_GTK_OL
#ifdef CONFIG_PNO_SUPPORT
	u8 LocPNOInfo;
	u8 LocScanInfo;
	u8 LocSSIDInfo;
	u8 LocProbePacket;
#endif //CONFIG_PNO_SUPPORT
#endif //CONFIG_WOWLAN	
#ifdef CONFIG_AP_WOWLAN
	u8 LocApOffloadBCN;
#endif //CONFIG_AP_WOWLAN
} RSVDPAGE_LOC, *PRSVDPAGE_LOC;

#endif
