#ifndef MEDIA_CLIENT_H
#define MEDIA_CLIENT_H

#include <mutex>
#include "ENetClient.h"

class MediaClient
{
public:
	MediaClient();
	virtual ~MediaClient();

	bool Connect(const char* ip, uint16_t port, uint32_t timeout_msec = 5000);
	void Close();

private:
	void SendActive();
	void EventLoop();

	std::mutex mutex_;
	bool is_started_ = false;

	std::shared_ptr<std::thread> event_thread_;
	ENetClient event_client_;
};

#endif