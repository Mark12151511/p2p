#ifndef ENENT_CLIENT_H
#define ENENT_CLIENT_H

#include <cstdint>
#include "enet/enet.h"

class ENetClient
{
public:
	ENetClient();
	virtual ~ENetClient();

	bool Connect(const char* ip, uint16_t port, uint32_t timeout_msec = 5000);
	void Close();

	int Send(void* data, uint32_t size);
	int Recv(void* data, uint32_t size, uint32_t timeout_msec = 100);

private:
	ENetAddress address_;
	ENetHost* client_;
	ENetPeer* peer_;
};

#endif