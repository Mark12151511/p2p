#ifndef ENENT_SERVER_H
#define ENENT_SERVER_H

#include <cstdint>
#include <map>
#include "enet/enet.h"

class ENetServer
{
public:
	ENetServer();
	virtual ~ENetServer();

	bool Start(const char* ip, uint16_t port, uint32_t connections = 32);
	void Stop();

	bool IsConnected(uint32_t cid);

	int Send(uint32_t cid, void* data, uint32_t size);
	int Recv(uint32_t* cid, void* data, uint32_t size, uint32_t timeout_msec = 10);

private:
	ENetAddress address_;
	ENetHost* server_;
	std::map<uint32_t, ENetPeer*> connections_;
};

#endif
