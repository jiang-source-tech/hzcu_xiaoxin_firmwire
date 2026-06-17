#!/usr/bin/env python3
"""PC viewer for TFT mirror packets sent over USB CDC serial."""

from __future__ import annotations

import argparse
import base64
import queue
import struct
import threading
import time
import tkinter as tk
import zlib
from dataclasses import dataclass
from typing import Optional, Tuple

import serial


MAGIC = 0x4D544654  # "TFTM", little-endian
VERSION = 1
COLOR_RGB565 = 1
FLAG_PANEL_BYTES_SWAPPED = 0x0001

PKT_HELLO = 1
PKT_RECT = 2
PKT_FRAME_END = 3
PKT_FULL_FRAME = 4

HEADER = struct.Struct("<IBBHIII")
HELLO_PAYLOAD = struct.Struct("<HHHH")
RECT_PAYLOAD_HEADER = struct.Struct("<IHHHHH")
FRAME_END_PAYLOAD = struct.Struct("<III")

MAX_PAYLOAD_SIZE = 512 * 1024


class MirrorStreamError(Exception):
    """Recoverable mirror stream failure."""


@dataclass
class StreamState:
    width: int = 0
    height: int = 0
    swapped: bool = False
    framebuffer: Optional[bytearray] = None
    last_sequence: Optional[int] = None

    @property
    def framebuffer_size(self) -> int:
        return self.width * self.height * 2

    def require_framebuffer(self) -> bytearray:
        if self.framebuffer is None:
            raise MirrorStreamError("received frame data before HELLO")
        return self.framebuffer


def read_exact(port: serial.Serial, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = port.read(size - len(data))
        if not chunk:
            raise ConnectionError("USB CDC stream timed out")
        data.extend(chunk)
    return bytes(data)


def read_header_resync(port: serial.Serial, max_payload: int) -> Tuple[int, int, int, int, int, int, int]:
    window = bytearray()
    while True:
        window.extend(read_exact(port, 1))
        if len(window) > HEADER.size:
            del window[0]
        if len(window) != HEADER.size:
            continue

        fields = HEADER.unpack(window)
        magic, version, packet_type, header_size, _sequence, payload_size, _crc32 = fields
        if (
            magic == MAGIC
            and version == VERSION
            and packet_type in (PKT_HELLO, PKT_RECT, PKT_FRAME_END, PKT_FULL_FRAME)
            and header_size == HEADER.size
            and payload_size <= max_payload
        ):
            return fields


def assert_crc(payload: bytes, expected: int) -> None:
    actual = zlib.crc32(payload) & 0xFFFFFFFF
    if actual != expected:
        raise MirrorStreamError(f"packet crc mismatch: got {actual:08x}, expected {expected:08x}")


def rgb565_to_rgb888(pixel: int, swapped: bool) -> Tuple[int, int, int]:
    if swapped:
        pixel = ((pixel & 0x00FF) << 8) | ((pixel & 0xFF00) >> 8)
    red = ((pixel >> 11) & 0x1F) * 255 // 31
    green = ((pixel >> 5) & 0x3F) * 255 // 63
    blue = (pixel & 0x1F) * 255 // 31
    return red, green, blue


def rgb565_framebuffer_to_rgb888(framebuffer: bytes, swapped: bool) -> bytes:
    rgb = bytearray(len(framebuffer) // 2 * 3)
    out = 0
    for src in range(0, len(framebuffer), 2):
        pixel = framebuffer[src] | (framebuffer[src + 1] << 8)
        red, green, blue = rgb565_to_rgb888(pixel, swapped)
        rgb[out] = red
        rgb[out + 1] = green
        rgb[out + 2] = blue
        out += 3
    return bytes(rgb)


def photo_from_rgb888(width: int, height: int, rgb: bytes) -> tk.PhotoImage:
    ppm = f"P6\n{width} {height}\n255\n".encode("ascii") + rgb
    try:
        return tk.PhotoImage(data=ppm, format="PPM")
    except tk.TclError:
        encoded = base64.b64encode(ppm).decode("ascii")
        return tk.PhotoImage(data=encoded, format="PPM")


def apply_rect(state: StreamState, payload: bytes) -> int:
    if len(payload) < RECT_PAYLOAD_HEADER.size:
        raise MirrorStreamError("RECT payload is shorter than its header")

    frame_id, x, y, width, height, stride = RECT_PAYLOAD_HEADER.unpack_from(payload)
    framebuffer = state.require_framebuffer()

    if width == 0 or height == 0:
        raise MirrorStreamError("RECT has zero width or height")
    if x + width > state.width or y + height > state.height:
        raise MirrorStreamError(
            f"RECT out of bounds: x={x} y={y} w={width} h={height} for {state.width}x{state.height}"
        )
    if stride < width:
        raise MirrorStreamError(f"RECT stride {stride} is smaller than width {width}")

    pixels = payload[RECT_PAYLOAD_HEADER.size :]
    needed = ((height - 1) * stride + width) * 2
    if len(pixels) < needed:
        raise MirrorStreamError(f"RECT pixel data is short: got {len(pixels)}, need {needed}")

    row_bytes = width * 2
    for row in range(height):
        src = row * stride * 2
        dst = ((y + row) * state.width + x) * 2
        framebuffer[dst : dst + row_bytes] = pixels[src : src + row_bytes]

    return frame_id


def apply_full_frame(state: StreamState, payload: bytes) -> int:
    framebuffer = state.require_framebuffer()
    frame_id = 0
    pixels = payload

    if len(payload) == state.framebuffer_size + 4:
        frame_id = struct.unpack_from("<I", payload)[0]
        pixels = payload[4:]
    elif len(payload) != state.framebuffer_size:
        raise MirrorStreamError(
            f"FULL_FRAME payload size {len(payload)} does not match framebuffer size {state.framebuffer_size}"
        )

    framebuffer[:] = pixels[: state.framebuffer_size]
    return frame_id


def check_sequence(state: StreamState, sequence: int) -> None:
    if state.last_sequence is not None:
        expected = (state.last_sequence + 1) & 0xFFFFFFFF
        if sequence != expected:
            print(f"sequence gap: expected {expected}, got {sequence}; waiting for next valid frame")
    state.last_sequence = sequence


def put_event(events: queue.Queue, event: Tuple) -> None:
    try:
        events.put_nowait(event)
    except queue.Full:
        if event[0] != "frame":
            return
        try:
            events.get_nowait()
        except queue.Empty:
            pass
        try:
            events.put_nowait(event)
        except queue.Full:
            pass


def receive_loop(port: serial.Serial, args: argparse.Namespace, events: queue.Queue) -> None:
    state = StreamState()

    while True:
        header = read_header_resync(port, args.max_payload)
        _magic, _version, packet_type, _header_size, sequence, payload_size, crc32 = header
        payload = read_exact(port, payload_size)
        assert_crc(payload, crc32)
        check_sequence(state, sequence)

        if packet_type == PKT_HELLO:
            if len(payload) != HELLO_PAYLOAD.size:
                raise MirrorStreamError(f"HELLO payload has unexpected size {len(payload)}")
            width, height, color_format, flags = HELLO_PAYLOAD.unpack(payload)
            if color_format != COLOR_RGB565:
                raise MirrorStreamError(f"unsupported color format {color_format}")
            state.width = width
            state.height = height
            state.swapped = bool(flags & FLAG_PANEL_BYTES_SWAPPED)
            state.framebuffer = bytearray(state.framebuffer_size)
            print(f"hello {width}x{height} RGB565 swapped={int(state.swapped)}")
            put_event(events, ("status", f"hello {width}x{height} RGB565"))

        elif packet_type == PKT_RECT:
            apply_rect(state, payload)

        elif packet_type == PKT_FRAME_END:
            if len(payload) != FRAME_END_PAYLOAD.size:
                raise MirrorStreamError(f"FRAME_END payload has unexpected size {len(payload)}")
            frame_id, rect_count, framebuffer_crc32 = FRAME_END_PAYLOAD.unpack(payload)
            framebuffer = bytes(state.require_framebuffer())
            actual_crc32 = zlib.crc32(framebuffer) & 0xFFFFFFFF
            ok = actual_crc32 == framebuffer_crc32
            status = "ok" if ok else f"viewer={actual_crc32:08x}"
            print(f"frame={frame_id} crc={framebuffer_crc32:08x} rects={rect_count} {status}")
            put_event(
                events,
                (
                    "frame",
                    state.width,
                    state.height,
                    state.swapped,
                    framebuffer,
                    frame_id,
                    framebuffer_crc32,
                    actual_crc32,
                    ok,
                ),
            )

        elif packet_type == PKT_FULL_FRAME:
            frame_id = apply_full_frame(state, payload)
            framebuffer = bytes(state.require_framebuffer())
            actual_crc32 = zlib.crc32(framebuffer) & 0xFFFFFFFF
            print(f"full_frame={frame_id} crc={actual_crc32:08x}")
            put_event(
                events,
                (
                    "frame",
                    state.width,
                    state.height,
                    state.swapped,
                    framebuffer,
                    frame_id,
                    actual_crc32,
                    actual_crc32,
                    True,
                ),
            )


def serial_worker(args: argparse.Namespace, events: queue.Queue, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            with serial.Serial(args.serial, args.baud, timeout=args.timeout) as port:
                print(f"connected {args.serial}")
                put_event(events, ("status", f"connected {args.serial}"))
                receive_loop(port, args, events)
        except (serial.SerialException, ConnectionError, MirrorStreamError, ValueError) as exc:
            message = f"mirror disconnected: {exc}; reconnecting in {args.reconnect_delay:g}s"
            print(message)
            put_event(events, ("status", message))
            stop_event.wait(args.reconnect_delay)


def make_ui(args: argparse.Namespace) -> Tuple[tk.Tk, tk.Label, tk.StringVar]:
    root = tk.Tk()
    root.title(f"TFT Mirror - {args.serial}")
    image_label = tk.Label(root, bd=0)
    image_label.pack()
    status_text = tk.StringVar(value="waiting for mirror stream")
    status_label = tk.Label(root, textvariable=status_text, anchor="w")
    status_label.pack(fill="x")
    return root, image_label, status_text


def poll_events(root: tk.Tk, label: tk.Label, status_text: tk.StringVar, events: queue.Queue, args: argparse.Namespace) -> None:
    try:
        while True:
            event = events.get_nowait()
            kind = event[0]
            if kind == "status":
                status_text.set(event[1])
            elif kind == "frame":
                _kind, width, height, swapped, framebuffer, frame_id, expected_crc, actual_crc, ok = event
                rgb = rgb565_framebuffer_to_rgb888(framebuffer, swapped)
                image = photo_from_rgb888(width, height, rgb)
                if args.scale != 1:
                    image = image.zoom(args.scale, args.scale)
                label.configure(image=image)
                label.image = image
                crc_status = "ok" if ok else f"mismatch viewer={actual_crc:08x}"
                status_text.set(f"frame={frame_id} crc={expected_crc:08x} {crc_status}")
    except queue.Empty:
        pass

    root.after(args.ui_interval_ms, poll_events, root, label, status_text, events, args)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="View TFT mirror packets from a USB CDC serial port.")
    parser.add_argument("--serial", required=True, help="USB CDC serial port, for example COM7")
    parser.add_argument("--baud", type=int, default=2_000_000, help="CDC baud hint shown by host tools")
    parser.add_argument("--timeout", type=float, default=2.0, help="serial read timeout in seconds")
    parser.add_argument("--reconnect-delay", type=float, default=1.0, help="delay before reopening the serial port")
    parser.add_argument("--max-payload", type=int, default=MAX_PAYLOAD_SIZE, help="largest accepted packet payload")
    parser.add_argument("--scale", type=int, default=1, choices=(1, 2, 3, 4), help="integer display scale")
    parser.add_argument("--ui-interval-ms", type=int, default=16, help="Tk event polling interval")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    events: queue.Queue = queue.Queue(maxsize=8)
    stop_event = threading.Event()
    root, label, status_text = make_ui(args)

    worker = threading.Thread(target=serial_worker, args=(args, events, stop_event), daemon=True)
    worker.start()

    def on_close() -> None:
        stop_event.set()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.after(args.ui_interval_ms, poll_events, root, label, status_text, events, args)
    root.mainloop()


if __name__ == "__main__":
    main()
