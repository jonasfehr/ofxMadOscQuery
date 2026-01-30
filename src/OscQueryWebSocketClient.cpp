#include "OscQueryWebSocketClient.h"

using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::WebSocket;

OscQueryWebSocketClient::OscQueryWebSocketClient() = default;

OscQueryWebSocketClient::~OscQueryWebSocketClient() {
	disconnect();
}

bool OscQueryWebSocketClient::connect(const std::string & host, int port, MessageCallback cb) {
	disconnect();
	onMessage = std::move(cb);
	try {
		HTTPClientSession session(host, port);
		HTTPRequest req(HTTPRequest::HTTP_GET, "/", Poco::Net::HTTPMessage::HTTP_1_1);
		HTTPResponse resp;
		auto ws = std::make_unique<WebSocket>(session, req, resp);
		// Short receive timeout so the listener thread can exit quickly and doesn't block shutdown.
		ws->setReceiveTimeout(Poco::Timespan(0, 20000000)); // 20ms
		ws->setSendTimeout(Poco::Timespan(0, 100000000));
		{
			std::lock_guard<std::mutex> lock(socketMutex);
			socket = std::move(ws);
		}
		running = true;
		listenThread = std::thread(&OscQueryWebSocketClient::listen, this);
		return true;
	} catch (const std::exception & e) {
		ofLogWarning() << "WebSocket connect failed: " << e.what();
		return false;
	}
}

void OscQueryWebSocketClient::disconnect() {
	running = false;
	{
		std::lock_guard<std::mutex> lock(socketMutex);
		if (socket) {
			try {
				socket->shutdown();
			} catch (...) { }
			socket.reset();
		}
	}
	if (listenThread.joinable()) listenThread.join();
}

bool OscQueryWebSocketClient::sendText(const std::string & msg) {
	std::lock_guard<std::mutex> lock(socketMutex);
	if (!socket) return false;
	try {
		socket->sendFrame(msg.data(), (int)msg.size(), WebSocket::FRAME_TEXT);
		return true;
	} catch (const std::exception & e) {
		ofLogWarning() << "WebSocket send failed: " << e.what();
		return false;
	}
}

void OscQueryWebSocketClient::listen() {
	char buffer[8192];
	while (running) {
		int flags = 0;
		int n = 0;
		try {
			// Copy socket pointer under lock, then call receiveFrame without holding the mutex.
			Poco::Net::WebSocket* wsRaw = nullptr;
			{
				std::lock_guard<std::mutex> lock(socketMutex);
				wsRaw = socket.get();
			}
			if (!wsRaw) break;

			n = wsRaw->receiveFrame(buffer, sizeof(buffer), flags);
			if (n <= 0) continue;

			const int op = (flags & WebSocket::FRAME_OP_BITMASK);
			if (op == WebSocket::FRAME_OP_CLOSE) break;

			// OSCQuery uses JSON-ish payloads; ignore unexpected binary frames.
			if (op == WebSocket::FRAME_OP_TEXT) {
				if (onMessage) onMessage(std::string(buffer, buffer + n));
			}
		} catch (const Poco::TimeoutException&) {
			// Expected idle; keep looping while "running".
			continue;
		} catch (const std::exception& e) {
			if (running) {
				ofLogWarning() << "WebSocket recv failed: " << e.what();
			}
			break;
		}
	}
	running = false;
}
