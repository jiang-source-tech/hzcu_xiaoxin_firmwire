#include "display_mirror_protocol.h"

tftm_packet_header_t tftm_make_header(uint8_t type, uint32_t sequence, uint32_t payload_size, uint32_t crc32) {
    tftm_packet_header_t header;
    header.magic = TFTM_MAGIC;
    header.version = TFTM_VERSION;
    header.type = type;
    header.header_size = (uint16_t)sizeof(tftm_packet_header_t);
    header.sequence = sequence;
    header.payload_size = payload_size;
    header.crc32 = crc32;
    return header;
}

bool tftm_rect_in_bounds(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t screen_w, uint16_t screen_h) {
    if (w == 0 || h == 0) {
        return false;
    }

    return x < screen_w && y < screen_h && w <= (uint16_t)(screen_w - x) && h <= (uint16_t)(screen_h - y);
}

uint32_t tftm_crc32(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}
