#include <iostream>
#include <string>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "dLogger.h"
#include "Database.h"
#include "dConfig.h"
#include "dMessageIdentifiers.h"
#include "dChatFilter.h"
#include "Diagnostics.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"

#include "PlayerContainer.h"
#include "ChatPackets.h"
#include "ChatPacketHandler.h"
#include "PacketUtils.h"

#include "irc/ircbot.h"

#include "Game.h"

//RakNet includes:
#include "RakNetDefines.h"
namespace Game {
	dLogger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	dChatFilter* chatFilter = nullptr;
	AssetManager* assetManager = nullptr;
	bool shouldShutdown = false;
}


dLogger* SetupLogger();
void HandlePacket(Packet* packet);
void SendIRCMessageToWorlds(const SystemAddress& sysAddr, const RakNet::RakString & sender_rak, const RakNet::RakString& message_rak);

IRCBot *irc_bot = nullptr;
std::string bridge_channel = "#hacksoc-lego";
bool irc_privmsg_handler(Event *ev);

PlayerContainer playerContainer;

int main(int argc, char** argv) {
	constexpr uint32_t chatFramerate = mediumFramerate;
	constexpr uint32_t chatFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Chat");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	//Read our config:
	Game::config = new dConfig((BinaryPathFinder::GetBinaryDir() / "chatconfig.ini").string());
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	Game::logger->Log("ChatServer", "Starting Chat server...");
	Game::logger->Log("ChatServer", "Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("ChatServer", "Compiled on: %s", __TIMESTAMP__);

	try {
		std::string clientPathStr = Game::config->GetValue("client_location");
		if (clientPathStr.empty()) clientPathStr = "./res";
		std::filesystem::path clientPath = std::filesystem::path(clientPathStr);
		if (clientPath.is_relative()) {
			clientPath = BinaryPathFinder::GetBinaryDir() / clientPath;
		}

		Game::assetManager = new AssetManager(clientPath);
	} catch (std::runtime_error& ex) {
		Game::logger->Log("ChatServer", "Got an error while setting up assets: %s", ex.what());
		
		return EXIT_FAILURE;
	}

	//Connect to the MySQL Database
	std::string mysql_host = Game::config->GetValue("mysql_host");
	std::string mysql_database = Game::config->GetValue("mysql_database");
	std::string mysql_username = Game::config->GetValue("mysql_username");
	std::string mysql_password = Game::config->GetValue("mysql_password");

	try {
		Database::Connect(mysql_host, mysql_database, mysql_username, mysql_password);
	} catch (sql::SQLException& ex) {
		Game::logger->Log("ChatServer", "Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("ChatServer");
		delete Game::server;
		delete Game::logger;
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP;
	uint32_t masterPort = 1000;
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT ip, port FROM servers WHERE name='master';");
	auto res = stmt->executeQuery();
	while (res->next()) {
		masterIP = res->getString(1).c_str();
		masterPort = res->getInt(2);
	}

	delete res;
	delete stmt;

	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	uint32_t maxClients = 50;
	uint32_t ourPort = 1501;
	if (Game::config->GetValue("max_clients") != "") maxClients = std::stoi(Game::config->GetValue("max_clients"));
	if (Game::config->GetValue("port") != "") ourPort = std::atoi(Game::config->GetValue("port").c_str());

	Game::server = new dServer(Game::config->GetValue("external_ip"), ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Chat, Game::config, &Game::shouldShutdown);

	Game::chatFilter = new dChatFilter(Game::assetManager->GetResPath().string() + "/chatplus_en_us", bool(std::stoi(Game::config->GetValue("dont_generate_dcf"))));

	// Insert bot code
	ConnectionDispatcher bot_connection;

	Data::add_type("string", new StringType());
	Data::add_type("int", new IntType());
	Data::add_type("pair", new PairType());
	Data::add_type("list", new ListType());
	Data::add_type("map", new MapType());

	Config *bot_config = nullptr;
	Config *bot_locale = nullptr;
	// Create IRC client
	try {
		bot_config = new Config("bot_config.cfg");
		bot_locale = new Config("bot_locale.cfg");
	} catch(ConfigException e) {
		Game::logger->Log("IRCClient", e.m_message);
		return -1;
	}

	irc_bot = new IRCBot(bot_config, bot_locale);
	irc_bot->connect(&bot_connection);
	irc_bot->add_handler("irc/privmsg", "chat", irc_privmsg_handler);

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 30 * chatFramerate; // 30 seconds in frames
	constexpr uint32_t sqlPingTime = 10 * 60 * chatFramerate; // 10 minutes in frames
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceLastSQLPing = 0;

	while (!Game::shouldShutdown) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= chatFramerate)
				break; //Exit our loop, shut down.
		} else framesSinceMasterDisconnect = 0;

		//In world we'd update our other systems here.

		//Check for packets here:
		Game::server->ReceiveFromMaster(); //ReceiveFromMaster also handles the master packets if needed.
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}

		//Push our log every 30s:
		if (framesSinceLastFlush >= logFlushTime) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else framesSinceLastFlush++;

		//Every 10 min we ping our sql server to keep it alive hopefully:
		if (framesSinceLastSQLPing >= sqlPingTime) {
			//Find out the master's IP for absolutely no reason:
			std::string masterIP;
			uint32_t masterPort;
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT ip, port FROM servers WHERE name='master';");
			auto res = stmt->executeQuery();
			while (res->next()) {
				masterIP = res->getString(1).c_str();
				masterPort = res->getInt(2);
			}

			delete res;
			delete stmt;

			framesSinceLastSQLPing = 0;
		} else framesSinceLastSQLPing++;

		bot_connection.handle();

		//Sleep our thread since auth can afford to.
		t += std::chrono::milliseconds(chatFrameDelta); //Chat can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	Data::cleanup_types();
	delete irc_bot;

	//Delete our objects here:
	Database::Destroy("ChatServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

dLogger* SetupLogger() {
	std::string logPath = (BinaryPathFinder::GetBinaryDir() / ("logs/ChatServer_" + std::to_string(time(nullptr)) + ".log")).string();
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
}

bool irc_privmsg_handler(Event *ev) {
	IRCMessageEvent *irc_message = reinterpret_cast<IRCMessageEvent*>(ev);
	if(irc_message->target == bridge_channel) {
		std::string new_nick = irc_message->sender->nick + " (IRC)";
		RakNet::RakString sender_rak(new_nick.c_str());
		RakNet::RakString message_rak(irc_message->message.c_str());
		SendIRCMessageToWorlds(UNASSIGNED_SYSTEM_ADDRESS, sender_rak, message_rak);
	}
	return true;
}

void SendIRCMessageToWorlds(const SystemAddress& sysAddr, const RakNet::RakString & sender_rak, const RakNet::RakString& message_rak) {
    CBITSTREAM;
    PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_IRC_MESSAGE);

    bitStream.Write(sender_rak);
    bitStream.Write(message_rak);

    Game::server->Send(&bitStream, sysAddr, true); //send to everyone except origin
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		Game::logger->Log("ChatServer", "A server has disconnected, erasing their connected players from the list.");
	}

	if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		Game::logger->Log("ChatServer", "A server is connecting, awaiting user list.");
	}

	if (packet->length >= 4 && packet->data[1] == CHAT_INTERNAL) {
		switch (packet->data[3]) {
		case MSG_CHAT_INTERNAL_PLAYER_ADDED_NOTIFICATION:
			playerContainer.InsertPlayer(packet);
			break;

		case MSG_CHAT_INTERNAL_PLAYER_REMOVED_NOTIFICATION:
			playerContainer.RemovePlayer(packet);
			break;

		case MSG_CHAT_INTERNAL_MUTE_UPDATE:
			playerContainer.MuteUpdate(packet);
			break;

		case MSG_CHAT_INTERNAL_CREATE_TEAM:
			playerContainer.CreateTeamServer(packet);
			break;

		case MSG_CHAT_INTERNAL_ANNOUNCEMENT: {
			//we just forward this packet to every connected server
			CINSTREAM;
			Game::server->Send(&inStream, packet->systemAddress, true); //send to everyone except origin
			break;
		}

		case MSG_CHAT_INTERNAL_IRC_MESSAGE: {
			// Forward this on as well. We also want to forward it to IRC, so to do this I'm going to reconstruct the packet... hold on

			CINSTREAM;
			LWOOBJID header;
            inStream.Read(header);

            RakNet::RakString user;
            RakNet::RakString msg;

            inStream.Read(user);
            inStream.Read(msg);

            // Send to other world servers
     		SendIRCMessageToWorlds(packet->systemAddress, user, msg);

            // Send to IRC
            std::string irc_message = "<" + std::string(user.C_String()) + "> " + msg.C_String();
            irc_bot->send_message(bridge_channel, irc_message);

			break;
		}

		default:
			Game::logger->Log("ChatServer", "Unknown CHAT_INTERNAL id: %i", int(packet->data[3]));
		}
	}

	if (packet->length >= 4 && packet->data[1] == CHAT) {
		switch (packet->data[3]) {
		case MSG_CHAT_GET_FRIENDS_LIST:
			ChatPacketHandler::HandleFriendlistRequest(packet);
			break;

		case MSG_CHAT_GET_IGNORE_LIST:
			Game::logger->Log("ChatServer", "Asked for ignore list, but is unimplemented right now.");
			break;

		case MSG_CHAT_TEAM_GET_STATUS:
			ChatPacketHandler::HandleTeamStatusRequest(packet);
			break;

		case MSG_CHAT_ADD_FRIEND_REQUEST:
			//this involves someone sending the initial request, the response is below, response as in from the other player.
			//We basically just check to see if this player is online or not and route the packet.
			ChatPacketHandler::HandleFriendRequest(packet);
			break;

		case MSG_CHAT_ADD_FRIEND_RESPONSE:
			//This isn't the response a server sent, rather it is a player's response to a received request.
			//Here, we'll actually have to add them to eachother's friend lists depending on the response code.
			ChatPacketHandler::HandleFriendResponse(packet);
			break;

		case MSG_CHAT_REMOVE_FRIEND:
			ChatPacketHandler::HandleRemoveFriend(packet);
			break;

		case MSG_CHAT_GENERAL_CHAT_MESSAGE:
			ChatPacketHandler::HandleChatMessage(packet);
			break;

		case MSG_CHAT_PRIVATE_CHAT_MESSAGE:
			//This message is supposed to be echo'd to both the sender and the receiver
			//BUT: they have to have different responseCodes, so we'll do some of the ol hacky wacky to fix that right up.
			ChatPacketHandler::HandlePrivateChatMessage(packet);
			break;

		case MSG_CHAT_TEAM_INVITE:
			ChatPacketHandler::HandleTeamInvite(packet);
			break;

		case MSG_CHAT_TEAM_INVITE_RESPONSE:
			ChatPacketHandler::HandleTeamInviteResponse(packet);
			break;

		case MSG_CHAT_TEAM_LEAVE:
			ChatPacketHandler::HandleTeamLeave(packet);
			break;

		case MSG_CHAT_TEAM_SET_LEADER:
			ChatPacketHandler::HandleTeamPromote(packet);
			break;

		case MSG_CHAT_TEAM_KICK:
			ChatPacketHandler::HandleTeamKick(packet);
			break;

		case MSG_CHAT_TEAM_SET_LOOT:
			ChatPacketHandler::HandleTeamLootOption(packet);
			break;

		default:
			Game::logger->Log("ChatServer", "Unknown CHAT id: %i", int(packet->data[3]));
		}
	}

	if (packet->length >= 4 && packet->data[1] == WORLD) {
		switch (packet->data[3]) {
		case MSG_WORLD_CLIENT_ROUTE_PACKET: {
			Game::logger->Log("ChatServer", "Routing packet from world");
			break;
		}

		default:
			Game::logger->Log("ChatServer", "Unknown World id: %i", int(packet->data[3]));
		}
	}
}
