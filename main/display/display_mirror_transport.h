#ifndef DISPLAY_MIRROR_TRANSPORT_H
#define DISPLAY_MIRROR_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

class DisplayMirrorTransport {
public:
    virtual ~DisplayMirrorTransport() = default;
    virtual bool Start(uint16_t width, uint16_t height, uint16_t flags) = 0;
    virtual bool HasClient() const = 0;
    virtual bool SendPacket(uint8_t type, const void* payload, size_t payload_size) = 0;
};

DisplayMirrorTransport* GetDisplayMirrorTransport();

#endif
