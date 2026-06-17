#ifndef DISPLAY_MIRROR_PROTOCOL_H
#define DISPLAY_MIRROR_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFTM_MAGIC 0x4D544654u
#define TFTM_VERSION 1u
#define TFTM_COLOR_RGB565 1u
#define TFTM_FLAG_PANEL_BYTES_SWAPPED 0x0001u

#if defined(__GNUC__)
#define TFTM_PACKED __attribute__((packed))
#else
#define TFTM_PACKED
#endif

typedef enum {
    TFTM_PACKET_HELLO = 1,
    TFTM_PACKET_RECT = 2,
    TFTM_PACKET_FRAME_END = 3,
    TFTM_PACKET_FULL_FRAME = 4,
} tftm_packet_type_t;

typedef struct TFTM_PACKED {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t header_size;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t crc32;
} tftm_packet_header_t;

typedef struct TFTM_PACKED {
    uint16_t width;
    uint16_t height;
    uint16_t color_format;
    uint16_t flags;
} tftm_hello_payload_t;

typedef struct TFTM_PACKED {
    uint32_t frame_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t stride_pixels;
} tftm_rect_payload_header_t;

typedef struct TFTM_PACKED {
    uint32_t frame_id;
    uint32_t rect_count;
    uint32_t framebuffer_crc32;
} tftm_frame_end_payload_t;

tftm_packet_header_t tftm_make_header(uint8_t type, uint32_t sequence, uint32_t payload_size, uint32_t crc32);
bool tftm_rect_in_bounds(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t screen_w, uint16_t screen_h);
uint32_t tftm_crc32(const void* data, size_t size);

#undef TFTM_PACKED

#ifdef __cplusplus
}
#endif

#endif
