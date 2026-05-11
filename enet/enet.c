#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "enet.h"

int enet_initialize(void) { return 0; }
void enet_deinitialize(void) {}

void enet_address_set_host(ENetAddress *address, const char *host) {
    if (strcmp(host, "127.0.0.1") == 0) {
        address->host[0] = 127;
        address->host[1] = 0;
        address->host[2] = 0;
        address->host[3] = 1;
    } else {
        memset(address->host, 0, 16);
    }
}

ENetHost * enet_host_create(const ENetAddress *a, size_t c, size_t b, unsigned int, unsigned int) {
    return (ENetHost*)1;
}

void enet_host_destroy(ENetHost *h) {}

int enet_host_service(ENetHost *h, ENetEvent *e, unsigned int t) {
    return 0;
}

void enet_host_flush(ENetHost *h) {}

ENetPeer * enet_host_connect(ENetHost *h, const ENetAddress *a, size_t c, unsigned int d) {
    return (ENetPeer*)1;
}

int enet_peer_send(ENetPeer *p, unsigned char ch, ENetPacket *pk) {
    return 0;
}

void enet_peer_disconnect(ENetPeer *p, unsigned int d) {}

ENetPacket * enet_packet_create(const void *dat, size_t len, unsigned int flag) {
    ENetPacket *pkt = (ENetPacket*)malloc(sizeof(ENetPacket) + len);
    if (pkt) {
        pkt->dataLength = len;
        pkt->flags = flag;
        pkt->data = pkt + 1;
        memcpy(pkt->data, dat, len);
    }
    return pkt;
}

void enet_packet_destroy(ENetPacket *p) {
    if (p) free(p);
}