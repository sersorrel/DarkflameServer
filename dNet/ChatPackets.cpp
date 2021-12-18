/*
 * Darkflame Universe
 * Copyright 2018
 */

#include <map>
#include "ChatPackets.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "Game.h"
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"
#include "dServer.h"
#include "dZoneManager.h"

std::map<int, std::string> zone_map = {
{1000, "Venture Explorer"},
{1001, "Return to Venture Explorer"},
{1100, "Avant Gardens"},
{1101, "Avant Gardens Survival"},
{1102, "Spider Queen Battle"},
{1150, "Block Yard"},
{1151, "Avant Grove"},
{1200, "Nimbus Station"},
{1201, "Pet Cove"},
{1203, "Vertigo Loop Racetrack"},
{1204, "Battle of Nimbus Station"},
{1250, "Nimbus Rock"},
{1251, "Nimbus Isle"},
{1300, "Gnarled Forest"},
{1302, "Canyon Cove"},
{1303, "Keelhaul Canyon"},
{1350, "Chantey Shantey"},
{1400, "Forbidden Valley"},
{1402, "Forbidden Valley Dragon"},
{1403, "Dragonmaw Chasm"},
{1450, "Raven Bluff"},
{1600, "Starbase 3001"},
{1601, "Deep Freeze"},
{1602, "Robot City"},
{1603, "Moon Base"},
{1604, "Portabello"},
{1700, "LEGO Club"},
{1800, "Crux Prime"},
{1900, "Nexus Tower"},
{2000, "Ninjago"},
{2001, "Frakjaw Battle"}
};

void ChatPackets::SendIRCMessageToChat(const std::string& sender, const std::string& message) {
    CBITSTREAM;
    PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_IRC_MESSAGE);

    Zone *zone = dZoneManager::Instance()->GetZone();
    int zone_id = zone->GetZoneID().GetMapID();
    std::string zone_name;

    if(zone_map.count(zone_id) != 0) {
      zone_name = zone_map[zone_id];
    } else {
      zone_name = zone->GetZoneDesc();
    }

    std::string sender_with_zone = sender + " (" + zone_name + ")";

    RakNet::RakString sender_rak(sender_with_zone.c_str());
    RakNet::RakString message_rak(message.c_str());

    bitStream.Write(sender_rak);
    bitStream.Write(message_rak);

    Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
}

void ChatPackets::SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message) {
    CBITSTREAM
    PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_GENERAL_CHAT_MESSAGE);
    
    bitStream.Write(static_cast<uint64_t>(0));
    bitStream.Write(chatChannel);
    
    bitStream.Write(static_cast<uint32_t>(message.size()));
    PacketUtils::WriteWString(bitStream, senderName, 33);

    bitStream.Write(playerObjectID);
    bitStream.Write(static_cast<uint16_t>(0));
    bitStream.Write(static_cast<char>(0));

    for (uint32_t i = 0; i < message.size(); ++i) {
        bitStream.Write(static_cast<uint16_t>(message[i]));
    }
    bitStream.Write(static_cast<uint16_t>(0));
    
    SEND_PACKET_BROADCAST
}

void ChatPackets::SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast) {
    CBITSTREAM
    PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_GENERAL_CHAT_MESSAGE);
    
    bitStream.Write(static_cast<uint64_t>(0));
    bitStream.Write(static_cast<char>(4));

    bitStream.Write(static_cast<uint32_t>(message.size()));
    PacketUtils::WriteWString(bitStream, "", 33);

    bitStream.Write(static_cast<uint64_t>(0));
    bitStream.Write(static_cast<uint16_t>(0));
    bitStream.Write(static_cast<char>(0));

    for (uint32_t i = 0; i < message.size(); ++i) {
        bitStream.Write(static_cast<uint16_t>(message[i]));
    }

    bitStream.Write(static_cast<uint16_t>(0));
    
	//This is so Wincent's announcement works:
	if (sysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
		SEND_PACKET;
		return;
	}
	
	SEND_PACKET_BROADCAST;
}

void ChatPackets::SendMessageFail(const SystemAddress& sysAddr) {
    //0x00 - "Chat is currently disabled."
    //0x01 - "Upgrade to a full LEGO Universe Membership to chat with other players."

    CBITSTREAM;
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_SEND_CANNED_TEXT);
    bitStream.Write<uint8_t>(0); //response type, options above ^
    //docs say there's a wstring here-- no idea what it's for, or if it's even needed so leaving it as is for now.
    SEND_PACKET;
}
