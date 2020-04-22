#ifndef MEDIA_SERVER_H
#define MEDIA_SERVER_H

#include <mutex>
#include <thread>
#include <memory>
#include "ENetServer.h"

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
	void SendActiveAck(uint32_t cid, uint32_t uid, uint32_t cseq);
	void EventLoop();

	std::mutex mutex_;
	bool is_started_ = false;

	std::shared_ptr<std::thread> event_thread_;
	ENetServer event_server_;

	static const int kMaxConnectios = 2;
};

#endif