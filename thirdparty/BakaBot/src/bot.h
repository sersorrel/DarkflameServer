#pragma once
#include "event/bot_events.h"
#include "bot_config.h"
#include <thread>

class Bot : public EventSink
{
public:
	Bot();
	Bot(Config *c, Config *l);
	virtual ~Bot();

	virtual void connect(ConnectionDispatcher *d);

	bool should_stop;

	Config *config;
    Config *locale;

    virtual void __connect(ConnectionDispatcher *d, std::string server, short port, bool use_ssl) = 0;
    virtual void join(std::string channel) = 0;
    virtual void leave(std::string channel) = 0;
    virtual void quit(std::string message) = 0;
    virtual void send_message(std::string target, std::string message) = 0;

	std::string type;

	bool cb_privmsg(Event *e);
protected:
	void init_plugins();
	void destroy_plugins();

	void end_sasl();
	bool cb_sasl_done(Event *e);

	void event_thread_func();
	std::thread event_thread;
};
