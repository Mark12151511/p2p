#include "ENetServer.h"

ENetServer::ENetServer()
	: server_(nullptr)
{
	memset(&address_, 0, sizeof(ENetAddress));
}

ENetServer::~ENetServer()
{

}

bool ENetServer::Start(const char* ip, uint16_t port, uint32_t connections)
{
	Stop();

	enet_address_set_host(&address_, ip);
	address_.port = port;

	server_ = enet_host_create(&address_ /* the address to bind the server host to */,
								connections /* allow up to 32 clients and/or outgoing connections */,
								2 /* allow up to 2 channels to be used, 0 and 1 */,
								0 /* assume any amount of incoming bandwidth */,
								0 /* assume any amount of outgoing bandwidth */);
	if (server_ == nullptr) {
		return false;
	}

	return true;
}

void ENetServer::Stop()
{
	if (server_ != nullptr) {
		enet_host_destroy(server_);
		server_ = nullptr;
	}
}

int ENetServer::Send(uint32_t cid, void* data, uint32_t size)
{
	if (server_ == nullptr) {
		return -1;
	}

	auto iter = connections_.find(cid);
	if (iter == connections_.end()) {
		return -1;
	}

	ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(iter->second, 0, packet);
	enet_host_flush(server_);
	return 0;
}

int ENetServer::Recv(uint32_t* cid, void* data, uint32_t size, uint32_t timeout_msec)
{
	if (server_ == nullptr) {
		return -1;
	}

	ENetEvent event;
	int num_events = enet_host_service(server_, &event, timeout_msec);
	if (num_events < 0 || event.type == ENET_EVENT_TYPE_NONE) {
		return 0;
	}

	ENetPeer *peer = event.peer;

	switch (event.type) 
	{
	case ENET_EVENT_TYPE_CONNECT:
		connections_[peer->connectID] = peer;
		break;
	case ENET_EVENT_TYPE_DISCONNECT:
		connections_.erase(peer->connectID);
		break;
	case ENET_EVENT_TYPE_RECEIVE:
		{
			size_t copy = size;
			*cid = peer->connectID;
			if (event.packet->dataLength < size) {
				copy = event.packet->dataLength;
			}
			memcpy(data, event.packet->data, copy);
			enet_packet_destroy(event.packet);
			return copy;
		}
		break;
	case ENET_EVENT_TYPE_NONE:
		break;
	}

	return 0;
}
