#ifndef ENET_H
#define ENET_H

#include <stddef.h>
#include <sys/types.h>

typedef enum _ENetPeerState {
    ENET_PEER_STATE_DISCONNECTED,
    ENET_PEER_STATE_CONNECTING,
    ENET_PEER_STATE_ACKNOWLEDGING_CONNECT,
    ENET_PEER_STATE_CONNECTED,
    ENET_PEER_STATE_DISCONNECTING,
    ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT,
    ENET_PEER_STATE_ZOMBIE
} ENetPeerState;

typedef enum _ENetEventType {
    ENET_EVENT_TYPE_NONE,
    ENET_EVENT_TYPE_CONNECT,
    ENET_EVENT_TYPE_DISCONNECT,
    ENET_EVENT_TYPE_RECEIVE
} ENetEventType;

typedef struct _ENetAddress {
    unsigned char host[16];
    unsigned short port;
} ENetAddress;

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

typedef struct _ENetPacket {
    size_t dataLength;
    void * data;
    unsigned int flags;
} ENetPacket;

typedef struct _ENetEvent {
    ENetEventType type;
    ENetPeer * peer;
    ENetPacket * packet;
    unsigned int data;
} ENetEvent;

#define ENET_PACKET_FLAG_RELIABLE 1

int enet_initialize(void);
void enet_deinitialize(void);

void enet_address_set_host(ENetAddress *, const char *);

ENetHost * enet_host_create(const ENetAddress *, size_t, size_t, unsigned int, unsigned int);
void enet_host_destroy(ENetHost *);

int enet_host_service(ENetHost *, ENetEvent *, unsigned int);
void enet_host_flush(ENetHost *);

ENetPeer * enet_host_connect(ENetHost *, const ENetAddress *, size_t, unsigned int);

int enet_peer_send(ENetPeer *, unsigned char, ENetPacket *);
void enet_peer_disconnect(ENetPeer *, unsigned int);

ENetPacket * enet_packet_create(const void *, size_t, unsigned int);
void enet_packet_destroy(ENetPacket *);

#endif