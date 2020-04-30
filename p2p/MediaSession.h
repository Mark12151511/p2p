#ifndef MEDIA_SESSION_H
#define MEDIA_SESSION_H

#include "RtpSink.h"

class MediaSession
{
public:
	MediaSession(asio::io_service& io_service);
	virtual ~MediaSession();

	bool Open();
	void Close();

private:
	asio::io_service& io_service_;
	std::unique_ptr<RtpSink> rtp_sink_;
};

#endif