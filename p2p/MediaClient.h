#ifndef MEDIA_CLIENT_H
#define MEDIA_CLIENT_H

#include <mutex>
#include <atomic>
#include "ENetClient.h"

class MediaClient
{
public:
	MediaClient();
	virtual ~MediaClient();

	bool Connect(const char* ip, uint16_t port, uint32_t timeout_msec = 5000);
	void Close();
	bool IsConnected();

private:
	void Start();
	void Stop();
	void PollEvent(bool once = false, uint32_t timeout_msec = 100);
	bool OnMessage(const char* message, uint32_t len);
	void SendActive();

	std::mutex mutex_;
	bool is_started_ = false;

	std::shared_ptr<std::thread> event_thread_;
	ENetClient event_client_;

	std::atomic_bool is_active_;
	std::atomic_bool is_setup_;
	std::atomic_bool is_play_;
};

#endif