#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../main/display/display_mirror_protocol.h"

static void header_is_little_endian_and_sized(void) {
    tftm_packet_header_t h = tftm_make_header(TFTM_PACKET_HELLO, 7, 12, 0x12345678u);
    const uint8_t* bytes = (const uint8_t*)&h;

    assert(h.magic == TFTM_MAGIC);
    assert(h.version == TFTM_VERSION);
    assert(h.type == TFTM_PACKET_HELLO);
    assert(h.header_size == sizeof(tftm_packet_header_t));
    assert(h.sequence == 7);
    assert(h.payload_size == 12);
    assert(h.crc32 == 0x12345678u);

    assert(sizeof(tftm_packet_header_t) == 20);
    assert(bytes[0] == 0x54);
    assert(bytes[1] == 0x46);
    assert(bytes[2] == 0x54);
    assert(bytes[3] == 0x4D);
    assert(bytes[8] == 7);
    assert(bytes[16] == 0x78);
    assert(bytes[17] == 0x56);
    assert(bytes[18] == 0x34);
    assert(bytes[19] == 0x12);
}

static void payload_structs_match_wire_sizes(void) {
    tftm_hello_payload_t hello = {
        .width = 412,
        .height = 412,
        .color_format = TFTM_COLOR_RGB565,
        .flags = TFTM_FLAG_PANEL_BYTES_SWAPPED,
    };

    assert(sizeof(tftm_hello_payload_t) == 8);
    assert(sizeof(tftm_rect_payload_header_t) == 14);
    assert(sizeof(tftm_frame_end_payload_t) == 12);
    assert(hello.color_format == TFTM_COLOR_RGB565);
    assert((hello.flags & TFTM_FLAG_PANEL_BYTES_SWAPPED) != 0);
}

static void rect_bounds_are_validated(void) {
    assert(tftm_rect_in_bounds(0, 0, 412, 412, 412, 412));
    assert(tftm_rect_in_bounds(10, 20, 30, 40, 412, 412));
    assert(!tftm_rect_in_bounds(411, 0, 2, 1, 412, 412));
    assert(!tftm_rect_in_bounds(0, 411, 1, 2, 412, 412));
    assert(!tftm_rect_in_bounds(0, 0, 0, 10, 412, 412));
}

static void crc32_detects_payload_change(void) {
    const uint8_t a[] = {1, 2, 3, 4};
    const uint8_t b[] = {1, 2, 3, 5};
    const uint8_t vector[] = "123456789";

    assert(tftm_crc32(a, sizeof(a)) != tftm_crc32(b, sizeof(b)));
    assert(tftm_crc32(a, sizeof(a)) == tftm_crc32(a, sizeof(a)));
    assert(tftm_crc32(vector, strlen((const char*)vector)) == 0xCBF43926u);
}

int main(void) {
    header_is_little_endian_and_sized();
    payload_structs_match_wire_sizes();
    rect_bounds_are_validated();
    crc32_detects_payload_change();
    puts("display_mirror_protocol tests passed");
    return 0;
}
