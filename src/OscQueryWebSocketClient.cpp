#include "OscQueryWebSocketClient.h"
#include <cstring>
#include <Poco/ByteOrder.h>

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

			if (op == WebSocket::FRAME_OP_TEXT) {
				if (onMessage) onMessage(std::string(buffer, buffer + n));
			} else if (op == WebSocket::FRAME_OP_BINARY) {
				// Parse OSC packet: always forward address, optionally include first float arg.
				const char* data = buffer;
				const char* end = buffer + n;
				const char* addrEnd = (const char*)memchr(data, '\0', end - data);
				if (!addrEnd) continue;
				std::string address(data, addrEnd);
				// align to 4 bytes
				const char* p = addrEnd + 1;
				while (((uintptr_t)p % 4) != 0 && p < end) ++p;
				if (p >= end) continue;
				const char* typeEnd = (const char*)memchr(p, '\0', end - p);
				if (!typeEnd) continue;
				std::string typeTag(p, typeEnd);
				p = typeEnd + 1;
				while (((uintptr_t)p % 4) != 0 && p < end) ++p;
				if (p >= end) continue;

				float fval = 0.f;
				bool gotVal = false;
				if (typeTag.size() >= 2 && typeTag[0] == ',' && typeTag[1] == 'f' && (p + 4) <= end) {
					uint32_t be = 0;
					std::memcpy(&be, p, 4);
					be = Poco::ByteOrder::fromBigEndian(be);
					std::memcpy(&fval, &be, 4);
					gotVal = true;
				}

				if (onMessage) {
					ofJson msg;
					msg["FULL_PATH"] = address;
					if (gotVal) {
						msg["VALUE"] = ofJson::array({fval});
					}
					onMessage(msg.dump());
				}
			}
		} catch (const Poco::TimeoutException&) {
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
