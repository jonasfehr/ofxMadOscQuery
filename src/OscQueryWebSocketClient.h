#pragma once

#include "ofMain.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/WebSocket.h>

class OscQueryWebSocketClient {
public:
	using MessageCallback = std::function<void(const std::string&)>;

	OscQueryWebSocketClient();
	~OscQueryWebSocketClient();

	// Connect to ws://host:port/, invoke cb on each text payload.
	bool connect(const std::string& host, int port, MessageCallback cb);
	void disconnect();

	// Send a raw text message (e.g., OSC subscription JSON) if connected.
	bool sendText(const std::string& msg);

private:
	void listen();

	std::unique_ptr<Poco::Net::WebSocket> socket;
	std::thread listenThread;
	std::atomic<bool> running { false };
	std::mutex socketMutex;
	MessageCallback onMessage;
};
