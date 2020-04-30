#ifndef MEDIA_SERVER_H
#define MEDIA_SERVER_H

#include <mutex>
#include <thread>
#include <memory>
#include "ENetServer.h"
#include "MediaSession.h"
#include "asio.hpp"

class MediaServer
{
public:
	MediaServer();
	virtual ~MediaServer();

	bool Start(const char* ip, uint16_t port);
	void Stop();

	//int SetBitrate();
	//int PushVideo();
	//int PushAudio();
	//int SetEventCallback();

private:
	void EventLoop();
	void OnMessage(uint32_t cid, const char* message, uint32_t len);
	void SendActiveAck(uint32_t cid, uint32_t uid, uint32_t cseq);
	
	std::mutex mutex_;
	bool is_started_ = false;

	std::shared_ptr<std::thread> event_thread_;
	ENetServer event_server_;

	asio::io_service io_service_;
	std::unique_ptr<asio::io_service::work> io_service_work_;

	typedef std::shared_ptr<MediaSession> MediaSessionPtr;
	std::map<uint32_t, MediaSessionPtr> media_sessions_;

	static const int kMaxConnectios = 2;
};

#endif