#pragma once

#include "sock_connection.h"

class Server
{
public:
	Server(Config *config, bool debug);
	~Server();	
	void start();
private:
	int create_listening_socket(unsigned short port);
	bool debug;
	Config *config;
	ConnectionDispatcher dispatcher;
};
