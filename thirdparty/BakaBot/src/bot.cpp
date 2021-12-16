#include "bot.h"
#include "util.h"
#include <iostream>

void Bot::event_thread_func()
{
	while(true)
	{
		if(should_stop)
		{
			break;
		}

		handle_event();
	}
}

Bot::Bot() : should_stop(false), config(NULL), locale(NULL), event_thread(std::bind(&Bot::event_thread_func, this))
{
}

Bot::Bot(Config *c, Config *l) : should_stop(false), config(c), locale(l), event_thread(std::bind(&Bot::event_thread_func, this))
{
}

Bot::~Bot()
{
	event_thread.join();

	delete config;
	delete locale;
	
}

void Bot::connect(ConnectionDispatcher *d)
{
	std::string server = config->get("server.host")->as_string();
	short port = (short)config->get("server.port")->as_int();
	bool ssl = (config->get("server.ssl")->as_string() == "true");

	__connect(d, server, port, ssl);
}
