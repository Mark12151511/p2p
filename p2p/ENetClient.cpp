#include "ENetClient.h"

ENetClient::ENetClient()
	: client_(nullptr)
	, peer_(nullptr)
{
	memset(&address_, 0, sizeof(ENetAddress));
}

ENetClient::~ENetClient()
{
	Close();
}

bool ENetClient::Connect(const char* ip, uint16_t port, uint32_t timeout_msec)
{
	Close();

	client_ = enet_host_create(nullptr /* create a client host */,
								1 /* only allow 1 outgoing connection */,
								2 /* allow up 2 channels to be used, 0 and 1 */,
								0 /* assume any amount of incoming bandwidth */,
								0 /* assume any amount of outgoing bandwidth */);

	if (client_ == nullptr) {
		return false;
	}

	enet_address_set_host(&address_, ip);
	address_.port = port;
	peer_ = enet_host_connect(client_, &address_, 2, 0);

	if (peer_ == nullptr) {
		return false;
	}

	ENetEvent event;
	if (enet_host_service(client_, &event, timeout_msec) > 0 &&
		event.type == ENET_EVENT_TYPE_CONNECT) {
		return true;
	}
	else {
		enet_peer_reset(peer_);		
	}

	Close();
	return false;
}

void ENetClient::Close()
{
	if (peer_ != nullptr) {
		enet_peer_disconnect(peer_, 0);
		peer_ = nullptr;
	}

	if (client_ != nullptr) {
		enet_host_destroy(client_);
		client_ = nullptr;
	}
}

int ENetClient::Send(void* data, uint32_t size)
{
	if (peer_ == nullptr) {
		return -1;
	}

	ENetPacket * packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(peer_, 0, packet);
	enet_host_flush(client_);
	return 0;
}

int ENetClient::Recv(void* data, uint32_t size, uint32_t timeout_msec)
{
	if (peer_ == nullptr) {
		return -1;
	}

	ENetEvent event;
	int num_events = enet_host_service(client_, &event, timeout_msec);
	if (num_events < 0 || event.type == ENET_EVENT_TYPE_NONE) {
		return 0;
	}

	if (event.type == ENET_EVENT_TYPE_RECEIVE) {
		size_t copy = size;
		if (event.packet->dataLength < size) {
			copy = event.packet->dataLength;
		}
		memcpy(data, event.packet->data, copy);
		enet_packet_destroy(event.packet);
		return copy;
	}
	else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
		Close();
		return -1;
	}

	return 0;
}