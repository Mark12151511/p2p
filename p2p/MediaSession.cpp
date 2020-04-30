#include "MediaSession.h"

MediaSession::MediaSession(asio::io_service& io_service)
	: io_service_(io_service)
	, rtp_sink_(new RtpSink(io_service))
{

}

MediaSession::~MediaSession()
{
	Close();
}

bool MediaSession::Open()
{
	return rtp_sink_->Open();
}

void MediaSession::Close()
{
	rtp_sink_->Close();
}