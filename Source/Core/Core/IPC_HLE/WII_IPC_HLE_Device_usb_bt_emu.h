// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <deque>
#include <queue>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/Wiimote.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/IPC_HLE/hci.h"

class CWII_IPC_HLE_WiiMote;
class PointerWrap;

struct SQueuedEvent
{
	u8 m_buffer[1024] = { 0 };
	u32 m_size = 0;
	u16 m_connectionHandle = 0;

	SQueuedEvent(u32 size, u16 handle);
	SQueuedEvent() = default;
};

// Important to remember that this class is for /dev/usb/oh1/57e/305 ONLY
// /dev/usb/oh1 -> internal usb bus
// 57e/305 -> VendorID/ProductID of device on usb bus
// This device is ONLY the internal Bluetooth module (based on BCM2045 chip)
class CWII_IPC_HLE_Device_usb_oh1_57e_305_emu final
	: public CWII_IPC_HLE_Device_usb_oh1_57e_305_base
{
public:
	CWII_IPC_HLE_Device_usb_oh1_57e_305_emu(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305_emu();

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtlV(u32 _CommandAddress) override;

	void Update() override;

	// Send ACL data back to Bluetooth stack
	void SendACLPacket(u16 connection_handle, const u8* data, u32 size);

	bool RemoteDisconnect(u16 _connectionHandle);

	std::vector<CWII_IPC_HLE_WiiMote> m_WiiMotes;
	CWII_IPC_HLE_WiiMote* AccessWiiMote(const bdaddr_t& _rAddr);
	CWII_IPC_HLE_WiiMote* AccessWiiMote(u16 _ConnectionHandle);

	void DoState(PointerWrap& p) override;

private:
	struct SHCICommandMessage
	{
		u8 bRequestType;
		u8 bRequest;
		u16 wValue;
		u16 wIndex;
		u16 wLength;

		u32 m_PayLoadAddr;
		u32 m_PayLoadSize;
		u32 m_Address;
	};

	bdaddr_t m_ControllerBD;

	// this is used to trigger connecting via ACL
	u8 m_ScanEnable = 0;

	SHCICommandMessage m_CtrlSetup;
	CtrlBuffer m_HCIEndpoint;
	std::deque<SQueuedEvent> m_EventQueue;

	u32 m_ACLSetup;
	CtrlBuffer m_ACLEndpoint;

	class ACLPool
	{
		struct Packet
		{
			u8 data[ACL_PKT_SIZE];
			u16 size;
			u16 conn_handle;
		};

		std::deque<Packet> m_queue;

	public:
		ACLPool() : m_queue() {}
		void Store(const u8* data, const u16 size, const u16 conn_handle);

		void WriteToEndpoint(CtrlBuffer& endpoint);

		bool IsEmpty() const { return m_queue.empty(); }
		// For SaveStates
		void DoState(PointerWrap& p) { p.Do(m_queue); }
	} m_acl_pool;

	u32 m_PacketCount[MAX_BBMOTES];
	u64 m_last_ticks = 0;

	// Send ACL data to a device (wiimote)
	void IncDataPacket(u16 _ConnectionHandle);
	void SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size);

	// Events
	void AddEventToQueue(const SQueuedEvent& _event);
	bool SendEventCommandStatus(u16 _Opcode);
	void SendEventCommandComplete(u16 opcode, const void* data, u32 data_size);
	bool SendEventInquiryResponse();
	bool SendEventInquiryComplete();
	bool SendEventRemoteNameReq(const bdaddr_t& _bd);
	bool SendEventRequestConnection(CWII_IPC_HLE_WiiMote& _rWiiMote);
	bool SendEventConnectionComplete(const bdaddr_t& _bd);
	bool SendEventReadClockOffsetComplete(u16 _connectionHandle);
	bool SendEventConPacketTypeChange(u16 _connectionHandle, u16 _packetType);
	bool SendEventReadRemoteVerInfo(u16 _connectionHandle);
	bool SendEventReadRemoteFeatures(u16 _connectionHandle);
	bool SendEventRoleChange(bdaddr_t _bd, bool _master);
	bool SendEventNumberOfCompletedPackets();
	bool SendEventAuthenticationCompleted(u16 _connectionHandle);
	bool SendEventModeChange(u16 _connectionHandle, u8 _mode, u16 _value);
	bool SendEventDisconnect(u16 _connectionHandle, u8 _Reason);
	bool SendEventRequestLinkKey(const bdaddr_t& _bd);
	bool SendEventLinkKeyNotification(const u8 num_to_send);

	// Execute HCI Message
	void ExecuteHCICommandMessage(const SHCICommandMessage& _rCtrlMessage);

	// OGF 0x01 - Link control commands and return parameters
	void CommandWriteInquiryMode(const u8* input);
	void CommandWritePageScanType(const u8* input);
	void CommandHostBufferSize(const u8* input);
	void CommandInquiryCancel(const u8* input);
	void CommandRemoteNameReq(const u8* input);
	void CommandCreateCon(const u8* input);
	void CommandAcceptCon(const u8* input);
	void CommandReadClockOffset(const u8* input);
	void CommandReadRemoteVerInfo(const u8* input);
	void CommandReadRemoteFeatures(const u8* input);
	void CommandAuthenticationRequested(const u8* input);
	void CommandInquiry(const u8* input);
	void CommandDisconnect(const u8* input);
	void CommandLinkKeyNegRep(const u8* input);
	void CommandLinkKeyRep(const u8* input);
	void CommandDeleteStoredLinkKey(const u8* input);
	void CommandChangeConPacketType(const u8* input);

	// OGF 0x02 - Link policy commands and return parameters
	void CommandWriteLinkPolicy(const u8* input);
	void CommandSniffMode(const u8* input);

	// OGF 0x03 - Host Controller and Baseband commands and return parameters
	void CommandReset(const u8* input);
	void CommandWriteLocalName(const u8* input);
	void CommandWritePageTimeOut(const u8* input);
	void CommandWriteScanEnable(const u8* input);
	void CommandWriteUnitClass(const u8* input);
	void CommandReadStoredLinkKey(const u8* input);
	void CommandWritePinType(const u8* input);
	void CommandSetEventFilter(const u8* input);
	void CommandWriteInquiryScanType(const u8* input);
	void CommandWriteLinkSupervisionTimeout(const u8* input);

	// OGF 0x04 - Informational commands and return parameters
	void CommandReadBufferSize(const u8* input);
	void CommandReadLocalVer(const u8* input);
	void CommandReadLocalFeatures(const u8* input);
	void CommandReadBDAdrr(const u8* input);

	// OGF 0x3F - Vendor specific
	void CommandVendorSpecific_FC4C(const u8* input, u32 size);
	void CommandVendorSpecific_FC4F(const u8* input, u32 size);

	static void DisplayDisconnectMessage(const int wiimoteNumber, const int reason);

#pragma pack(push, 1)
#define CONF_PAD_MAX_REGISTERED 10

	struct _conf_pad_device
	{
		u8 bdaddr[6];
		char name[0x40];
	};

	struct _conf_pads
	{
		u8 num_registered;
		_conf_pad_device registered[CONF_PAD_MAX_REGISTERED];
		_conf_pad_device active[MAX_BBMOTES];
		u8 unknown[0x45];
	};
#pragma pack(pop)
};
