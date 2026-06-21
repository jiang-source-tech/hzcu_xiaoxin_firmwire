# Xiaoxin Binary TTS Downlink Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the deployed Xiaoxin server speak through the existing firmware WebSocket binary audio path by streaming TTS as Opus frames after each assistant reply.

**Architecture:** Keep the firmware's existing microphone upload and binary Opus playback pipeline. The server will unwrap firmware v3 binary microphone frames for ASR, synthesize TTS, transcode the provider audio URL into 24 kHz mono Opus packets, wrap each packet with the firmware v3 binary frame format, and send `tts start -> tts sentence_start -> binary audio frames -> tts stop`.

**Tech Stack:** FastAPI WebSocket server in `D:\AI_Pet\hzcu_xiaoxin\web`; ESP-IDF C++ firmware in `D:\AI_Pet\hzcu_xiaoxin_firmwire`; Python `unittest`; firmware host tests via existing `tests/` patterns; external `ffmpeg` CLI for MP3/WAV/Ogg-to-Opus conversion.

## Global Constraints

- Do not overwrite existing uncommitted firmware changes in `D:\AI_Pet\hzcu_xiaoxin_firmwire`; stage only files changed by the executing task.
- Firmware OTA already points at the Xiaoxin service through `CONFIG_OTA_URL`; do not change the user's current Cloudflare URL unless explicitly asked.
- Firmware WebSocket protocol version is `3`; version 3 binary frames are `type:uint8`, `reserved:uint8`, `payload_size:uint16 big-endian`, then payload bytes.
- Firmware microphone Opus upload is 16 kHz mono, 60 ms frames.
- Server TTS downlink Opus stream should be 24 kHz mono, 60 ms frames to match the existing server hello default and firmware decoder resampling behavior.
- If TTS audio cannot be streamed, the device must still display the assistant text and receive `tts stop`.
- No provider exception, API key, ffmpeg stderr, or stack trace may be sent to the firmware.
- Every task must keep the existing HTTP OTA endpoint and `/xiaoxin/v1` WebSocket endpoint compatible with current firmware.

---

## File Structure

### Server Repository: `D:\AI_Pet\hzcu_xiaoxin`

- Create `web/firmware_binary.py`
  - Single responsibility: parse and build firmware WebSocket binary audio frames for protocol versions 0/raw, 2, and 3.
  - Public API:
    - `BinaryAudioFrameError(ValueError)`
    - `parse_audio_frame(frame: bytes, protocol_version: int) -> bytes`
    - `build_audio_frame(payload: bytes, protocol_version: int, timestamp: int = 0) -> bytes`
- Create `web/tts_audio_stream.py`
  - Single responsibility: download a provider `audio_url`, transcode it through ffmpeg into Ogg Opus, parse Ogg packets, and return raw Opus packets suitable for firmware binary streaming.
  - Public API:
    - `TtsAudioStreamConfig`
    - `TtsAudioStreamError(RuntimeError)`
    - `opus_packets_from_tts_payload(payload: Mapping[str, Any], config: TtsAudioStreamConfig | None = None) -> list[bytes]`
- Modify `web/asr_service.py`
  - Add ASR sample rate configuration and pass it to DashScope Recognition.
- Modify `web/server.py`
  - Track each WebSocket client's protocol version and microphone sample rate from `hello`.
  - Unwrap incoming binary frames before appending audio capture data.
  - Stream binary TTS packets between `tts start` and `tts stop`.
- Modify `web/firmware_protocol.py`
  - Add `tts_start() -> dict[str, str]`.
- Modify `web/.env.example`
  - Document ffmpeg path and binary TTS streaming toggles.
- Modify tests:
  - `web/tests/test_firmware_binary.py`
  - `web/tests/test_tts_audio_stream.py`
  - `web/tests/test_asr_service.py`
  - `web/tests/test_firmware_protocol.py`
  - `web/tests/test_firmware_server.py`

### Firmware Repository: `D:\AI_Pet\hzcu_xiaoxin_firmwire`

- Modify `main/protocols/protocol.h`
  - Add a small inline parser helper for protocol v3 binary frames so malformed server frames can be rejected safely and host-tested.
- Modify `main/protocols/websocket_protocol.cc`
  - Use the parser helper before pushing incoming v3 binary payloads to `AudioService`.
- Add `tests/websocket_binary_protocol_test.c`
  - Host-level tests for valid v3 frames, truncated headers, and payload length mismatch.
- Optionally modify `docs/update.md` or add a short note after implementation if the manual smoke flow changes.

---

### Task 1: Server Binary Frame Helpers And Incoming Firmware Audio Unwrap

**Files:**
- Create: `D:\AI_Pet\hzcu_xiaoxin\web\firmware_binary.py`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\server.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_binary.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`

**Interfaces:**
- Consumes: WebSocket `hello.version`, WebSocket binary messages.
- Produces:
  - `parse_audio_frame(frame: bytes, protocol_version: int) -> bytes`
  - `build_audio_frame(payload: bytes, protocol_version: int, timestamp: int = 0) -> bytes`
  - `app.state` does not change; per-socket local variable `client_protocol_version: int`.

- [ ] **Step 1: Write failing tests for protocol v3 parse/build**

Create `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_binary.py`:

```python
import struct
import unittest

import firmware_binary


class FirmwareBinaryTest(unittest.TestCase):
    def test_parse_protocol_3_audio_frame_returns_payload(self):
        frame = b"\x00\x00" + struct.pack("!H", 5) + b"abcde"

        self.assertEqual(
            firmware_binary.parse_audio_frame(frame, protocol_version=3),
            b"abcde",
        )

    def test_build_protocol_3_audio_frame_wraps_payload(self):
        self.assertEqual(
            firmware_binary.build_audio_frame(b"opus", protocol_version=3),
            b"\x00\x00\x00\x04opus",
        )

    def test_parse_protocol_3_rejects_truncated_header(self):
        with self.assertRaises(firmware_binary.BinaryAudioFrameError):
            firmware_binary.parse_audio_frame(b"\x00\x00\x00", protocol_version=3)

    def test_parse_protocol_3_rejects_payload_length_mismatch(self):
        frame = b"\x00\x00" + struct.pack("!H", 9) + b"short"

        with self.assertRaises(firmware_binary.BinaryAudioFrameError):
            firmware_binary.parse_audio_frame(frame, protocol_version=3)

    def test_protocol_0_keeps_raw_payload_for_manual_clients(self):
        self.assertEqual(
            firmware_binary.parse_audio_frame(b"raw-opus", protocol_version=0),
            b"raw-opus",
        )
        self.assertEqual(
            firmware_binary.build_audio_frame(b"raw-opus", protocol_version=0),
            b"raw-opus",
        )


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the failing binary helper tests**

Run from `D:\AI_Pet\hzcu_xiaoxin\web`:

```powershell
python -m unittest tests.test_firmware_binary
```

Expected: FAIL with `ModuleNotFoundError: No module named 'firmware_binary'`.

- [ ] **Step 3: Implement `firmware_binary.py`**

Create `D:\AI_Pet\hzcu_xiaoxin\web\firmware_binary.py`:

```python
from __future__ import annotations

import struct


class BinaryAudioFrameError(ValueError):
    pass


def parse_audio_frame(frame: bytes, protocol_version: int) -> bytes:
    payload = bytes(frame or b"")
    if protocol_version <= 0:
        return payload

    if protocol_version == 3:
        if len(payload) < 4:
            raise BinaryAudioFrameError("protocol 3 audio frame header is incomplete")
        frame_type, _reserved, payload_size = struct.unpack("!BBH", payload[:4])
        if frame_type != 0:
            raise BinaryAudioFrameError("protocol 3 binary frame is not audio")
        body = payload[4:]
        if len(body) != payload_size:
            raise BinaryAudioFrameError("protocol 3 audio payload length mismatch")
        return body

    if protocol_version == 2:
        if len(payload) < 16:
            raise BinaryAudioFrameError("protocol 2 audio frame header is incomplete")
        version, frame_type, _reserved, _timestamp, payload_size = struct.unpack("!HHIII", payload[:16])
        if version != 2 or frame_type != 0:
            raise BinaryAudioFrameError("protocol 2 binary frame is not audio")
        body = payload[16:]
        if len(body) != payload_size:
            raise BinaryAudioFrameError("protocol 2 audio payload length mismatch")
        return body

    return payload


def build_audio_frame(payload: bytes, protocol_version: int, timestamp: int = 0) -> bytes:
    body = bytes(payload or b"")
    if protocol_version <= 0:
        return body

    if protocol_version == 3:
        if len(body) > 0xFFFF:
            raise BinaryAudioFrameError("protocol 3 audio payload is too large")
        return struct.pack("!BBH", 0, 0, len(body)) + body

    if protocol_version == 2:
        return struct.pack("!HHIII", 2, 0, 0, int(timestamp or 0), len(body)) + body

    return body
```

- [ ] **Step 4: Run the binary helper tests**

Run:

```powershell
python -m unittest tests.test_firmware_binary
```

Expected: PASS.

- [ ] **Step 5: Add failing server test showing firmware v3 upload is unwrapped before ASR**

In `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`, add this method to `FirmwareServerWebSocketTest`:

```python
    def test_bound_device_voice_turn_unwraps_protocol_3_audio_before_asr(self):
        audio_dir = Path(self.temp_dir.name) / "audio"
        self.client = TestClient(server.create_app(self.store, audio_dir=audio_dir))
        user = self.store.create_user("alice", "secret")
        pairing = self.store.create_pairing_code("device-1", "client-1")
        self.store.bind_pairing_code(pairing["code"], user["user_id"])

        seen_audio = []

        def fake_transcribe(path, config=None):
            seen_audio.append(Path(path).read_bytes())
            return {"text": "\u4f60\u597d"}

        def fake_reply(user_id_arg, text, reply_provider=None):
            return {
                "reply": "\u6211\u5728",
                "speech": "\u6211\u5728",
                "expression": "smile",
            }

        original_transcribe = server.asr_service.transcribe_audio
        original_reply = server.conversation_core.reply_to_text
        original_tts = server.tts_service.synthesize_speech
        server.asr_service.transcribe_audio = fake_transcribe
        server.conversation_core.reply_to_text = fake_reply
        server.tts_service.synthesize_speech = lambda *args, **kwargs: {"tts_provider": "disabled"}
        try:
            with self.client.websocket_connect(
                "/xiaoxin/v1",
                headers={"Device-Id": "device-1", "Client-Id": "client-1", "Protocol-Version": "3"},
            ) as websocket:
                websocket.send_json({"type": "hello", "version": 3})
                websocket.receive_json()
                websocket.send_json({"type": "listen", "state": "start"})
                websocket.send_bytes(b"\x00\x00\x00\x03abc")
                websocket.send_json({"type": "listen", "state": "stop"})
                websocket.receive_json()
        finally:
            server.asr_service.transcribe_audio = original_transcribe
            server.conversation_core.reply_to_text = original_reply
            server.tts_service.synthesize_speech = original_tts

        self.assertEqual(seen_audio, [b"abc"])
```

- [ ] **Step 6: Run the new server test and verify it fails**

Run:

```powershell
python -m unittest tests.test_firmware_server.FirmwareServerWebSocketTest.test_bound_device_voice_turn_unwraps_protocol_3_audio_before_asr
```

Expected: FAIL because `seen_audio` contains the wrapped bytes, not `b"abc"`.

- [ ] **Step 7: Modify `server.py` to track client protocol version and unwrap binary frames**

In `D:\AI_Pet\hzcu_xiaoxin\web\server.py`, add:

```python
import firmware_binary
```

Inside `firmware_socket`, after parsing `first_message`, derive:

```python
        try:
            client_protocol_version = int(first_message.get("version") or websocket.headers.get("protocol-version") or 0)
        except (TypeError, ValueError):
            client_protocol_version = 0
```

Replace the direct append in the binary branch:

```python
                try:
                    audio_payload = firmware_binary.parse_audio_frame(
                        message["bytes"],
                        client_protocol_version,
                    )
                    audio_session.append(audio_payload)
                except firmware_binary.BinaryAudioFrameError:
                    LOGGER.warning("Ignoring malformed firmware audio frame")
```

Keep the existing `AudioCaptureLimitExceeded` handling around `audio_session.append(audio_payload)` so over-limit frames still produce the safe prompt.

- [ ] **Step 8: Run focused server tests**

Run:

```powershell
python -m unittest tests.test_firmware_binary tests.test_firmware_server
```

Expected: PASS.

- [ ] **Step 9: Commit Task 1**

Run from `D:\AI_Pet\hzcu_xiaoxin`:

```powershell
git add web/firmware_binary.py web/server.py web/tests/test_firmware_binary.py web/tests/test_firmware_server.py
git commit -m "feat: unwrap firmware websocket audio frames"
```

Expected: commit succeeds and no firmware repository files are staged.

---

### Task 2: Server ASR Sample Rate Alignment For Firmware Upload

**Files:**
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\asr_service.py`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\server.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_asr_service.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`

**Interfaces:**
- Consumes: client hello `audio_params.sample_rate`.
- Produces:
  - `AsrConfig.sample_rate: int`
  - `dataclasses.replace(app.state.asr_config, sample_rate=client_sample_rate)` at call time when a config exists.

- [ ] **Step 1: Add failing ASR config tests**

In `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_asr_service.py`, add:

```python
    def test_load_asr_config_defaults_to_firmware_upload_sample_rate(self):
        config = asr_service.load_asr_config({})

        self.assertEqual(config.sample_rate, 16000)

    def test_dashscope_recognition_uses_configured_sample_rate(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            audio_path = Path(temp_dir) / "turn.opus"
            audio_path.write_bytes(b"opus-bytes")
            config = asr_service.load_asr_config({
                "BAILIAN_ASR_ENABLED": "true",
                "BAILIAN_ASR_API_KEY": "test-key",
                "BAILIAN_ASR_SAMPLE_RATE": "16000",
            })

            with patch("dashscope.audio.asr.Recognition") as recognition_cls:
                recognition_cls.return_value.call.return_value = {
                    "output": {"text": "turn text"}
                }
                asr_service.transcribe_audio(audio_path, config)

        self.assertEqual(recognition_cls.call_args.kwargs["sample_rate"], 16000)
```

- [ ] **Step 2: Run the failing ASR tests**

Run:

```powershell
python -m unittest tests.test_asr_service.AsrServiceTest.test_load_asr_config_defaults_to_firmware_upload_sample_rate tests.test_asr_service.AsrServiceTest.test_dashscope_recognition_uses_configured_sample_rate
```

Expected: FAIL because `AsrConfig.sample_rate` does not exist.

- [ ] **Step 3: Implement `sample_rate` in `asr_service.py`**

Modify `AsrConfig`:

```python
@dataclass(frozen=True)
class AsrConfig:
    enabled: bool
    api_key: str
    model: str = "paraformer-realtime-v2"
    provider: str = "dashscope-paraformer"
    sample_rate: int = 16000
```

Add helper:

```python
def _env_int(env: Mapping[str, str], key: str, default: int) -> int:
    try:
        return int(_env_value(env, key, str(default)))
    except ValueError:
        return default
```

In `load_asr_config`, pass:

```python
sample_rate=_env_int(active_env, "BAILIAN_ASR_SAMPLE_RATE", 16000),
```

In `_call_dashscope_asr`, replace:

```python
sample_rate=24000,
```

with:

```python
sample_rate=config.sample_rate,
```

- [ ] **Step 4: Add failing server test for client hello sample rate override**

In `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`, add:

```python
    def test_voice_turn_passes_client_sample_rate_to_asr_config(self):
        audio_dir = Path(self.temp_dir.name) / "audio"
        base_config = server.asr_service.AsrConfig(
            enabled=True,
            api_key="test-key",
            sample_rate=24000,
        )
        self.client = TestClient(server.create_app(self.store, audio_dir=audio_dir, asr_config=base_config))
        user = self.store.create_user("alice", "secret")
        pairing = self.store.create_pairing_code("device-1", "client-1")
        self.store.bind_pairing_code(pairing["code"], user["user_id"])
        seen_sample_rates = []

        def fake_transcribe(path, config=None):
            seen_sample_rates.append(config.sample_rate)
            return {"text": "\u4f60\u597d"}

        original_transcribe = server.asr_service.transcribe_audio
        original_reply = server.conversation_core.reply_to_text
        original_tts = server.tts_service.synthesize_speech
        server.asr_service.transcribe_audio = fake_transcribe
        server.conversation_core.reply_to_text = lambda *args, **kwargs: {
            "reply": "\u6211\u5728",
            "speech": "\u6211\u5728",
            "expression": "smile",
        }
        server.tts_service.synthesize_speech = lambda *args, **kwargs: {"tts_provider": "disabled"}
        try:
            with self.client.websocket_connect(
                "/xiaoxin/v1",
                headers={"Device-Id": "device-1", "Client-Id": "client-1"},
            ) as websocket:
                websocket.send_json({
                    "type": "hello",
                    "version": 3,
                    "audio_params": {"format": "opus", "sample_rate": 16000, "channels": 1, "frame_duration": 60},
                })
                websocket.receive_json()
                websocket.send_json({"type": "listen", "state": "start"})
                websocket.send_bytes(b"\x00\x00\x00\x03abc")
                websocket.send_json({"type": "listen", "state": "stop"})
                websocket.receive_json()
        finally:
            server.asr_service.transcribe_audio = original_transcribe
            server.conversation_core.reply_to_text = original_reply
            server.tts_service.synthesize_speech = original_tts

        self.assertEqual(seen_sample_rates, [16000])
```

- [ ] **Step 5: Run the failing server sample-rate test**

Run:

```powershell
python -m unittest tests.test_firmware_server.FirmwareServerWebSocketTest.test_voice_turn_passes_client_sample_rate_to_asr_config
```

Expected: FAIL because the server still passes the base config sample rate.

- [ ] **Step 6: Modify `server.py` to use client upload sample rate for ASR**

Add:

```python
from dataclasses import replace
```

After `client_protocol_version`, parse:

```python
        client_audio_params = first_message.get("audio_params") if isinstance(first_message.get("audio_params"), dict) else {}
        try:
            client_audio_sample_rate = int(client_audio_params.get("sample_rate") or 16000)
        except (TypeError, ValueError):
            client_audio_sample_rate = 16000
```

Before calling `asr_service.transcribe_audio`, derive:

```python
                        asr_config = app.state.asr_config
                        if asr_config is not None:
                            asr_config = replace(asr_config, sample_rate=client_audio_sample_rate)
```

Then pass `asr_config` to `transcribe_audio`.

- [ ] **Step 7: Run ASR and firmware server tests**

Run:

```powershell
python -m unittest tests.test_asr_service tests.test_firmware_server
```

Expected: PASS.

- [ ] **Step 8: Commit Task 2**

Run from `D:\AI_Pet\hzcu_xiaoxin`:

```powershell
git add web/asr_service.py web/server.py web/tests/test_asr_service.py web/tests/test_firmware_server.py
git commit -m "fix: align firmware asr sample rate"
```

---

### Task 3: Server TTS Audio URL To Raw Opus Packet Stream

**Files:**
- Create: `D:\AI_Pet\hzcu_xiaoxin\web\tts_audio_stream.py`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\.env.example`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_tts_audio_stream.py`

**Interfaces:**
- Consumes: TTS payload from `tts_service.synthesize_speech`, especially `audio_url` and `audio_format`.
- Produces:
  - `TtsAudioStreamConfig`
  - `opus_packets_from_tts_payload(payload: Mapping[str, Any], config: TtsAudioStreamConfig | None = None) -> list[bytes]`

- [ ] **Step 1: Write Ogg parser and payload behavior tests**

Create `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_tts_audio_stream.py`:

```python
import struct
import unittest
from unittest.mock import patch

import tts_audio_stream


def ogg_page(packets: list[bytes], sequence: int) -> bytes:
    segment_table = b"".join(bytes([len(packet)]) for packet in packets)
    body = b"".join(packets)
    header = (
        b"OggS"
        + b"\x00"
        + b"\x00"
        + struct.pack("<Q", 0)
        + struct.pack("<I", 1)
        + struct.pack("<I", sequence)
        + struct.pack("<I", 0)
        + bytes([len(packets)])
    )
    return header + segment_table + body


class TtsAudioStreamTest(unittest.TestCase):
    def test_missing_audio_url_returns_empty_packets(self):
        self.assertEqual(tts_audio_stream.opus_packets_from_tts_payload({}), [])

    def test_ogg_parser_skips_opus_headers_and_returns_audio_packets(self):
        ogg = (
            ogg_page([b"OpusHead" + b"\x00"], 0)
            + ogg_page([b"OpusTags" + b"\x00"], 1)
            + ogg_page([b"audio-one", b"audio-two"], 2)
        )

        self.assertEqual(
            tts_audio_stream._opus_packets_from_ogg(ogg),
            [b"audio-one", b"audio-two"],
        )

    def test_transcode_downloaded_audio_to_opus_packets(self):
        ogg = (
            ogg_page([b"OpusHead" + b"\x00"], 0)
            + ogg_page([b"OpusTags" + b"\x00"], 1)
            + ogg_page([b"audio-one"], 2)
        )
        config = tts_audio_stream.TtsAudioStreamConfig(ffmpeg_path="ffmpeg-test")

        with patch.object(tts_audio_stream, "_download_audio_url", return_value=b"mp3-bytes") as download:
            with patch.object(tts_audio_stream, "_ffmpeg_to_ogg_opus", return_value=ogg) as transcode:
                packets = tts_audio_stream.opus_packets_from_tts_payload(
                    {"audio_url": "https://example.test/tts.mp3", "audio_format": "mp3"},
                    config,
                )

        download.assert_called_once_with("https://example.test/tts.mp3", config)
        transcode.assert_called_once_with(b"mp3-bytes", "mp3", config)
        self.assertEqual(packets, [b"audio-one"])

    def test_download_or_transcode_failure_returns_empty_packets(self):
        config = tts_audio_stream.TtsAudioStreamConfig()

        with patch.object(tts_audio_stream, "_download_audio_url", side_effect=RuntimeError("secret ffmpeg log")):
            packets = tts_audio_stream.opus_packets_from_tts_payload(
                {"audio_url": "https://example.test/tts.mp3", "audio_format": "mp3"},
                config,
            )

        self.assertEqual(packets, [])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run failing TTS stream tests**

Run from `D:\AI_Pet\hzcu_xiaoxin\web`:

```powershell
python -m unittest tests.test_tts_audio_stream
```

Expected: FAIL with `ModuleNotFoundError: No module named 'tts_audio_stream'`.

- [ ] **Step 3: Implement `tts_audio_stream.py`**

Create `D:\AI_Pet\hzcu_xiaoxin\web\tts_audio_stream.py`:

```python
from __future__ import annotations

import os
import subprocess
import urllib.request
from dataclasses import dataclass
from typing import Any, Mapping


@dataclass(frozen=True)
class TtsAudioStreamConfig:
    enabled: bool = True
    ffmpeg_path: str = "ffmpeg"
    target_sample_rate: int = 24000
    frame_duration_ms: int = 60
    download_timeout_seconds: float = 15.0
    transcode_timeout_seconds: float = 20.0
    max_download_bytes: int = 4 * 1024 * 1024


class TtsAudioStreamError(RuntimeError):
    pass


def load_tts_audio_stream_config(env: Mapping[str, str] | None = None) -> TtsAudioStreamConfig:
    active_env = env if env is not None else os.environ
    return TtsAudioStreamConfig(
        enabled=str(active_env.get("XIAOXIN_BINARY_TTS_ENABLED", "true")).strip().lower() in {"1", "true", "yes", "on"},
        ffmpeg_path=str(active_env.get("XIAOXIN_FFMPEG_PATH", "ffmpeg") or "ffmpeg").strip(),
        target_sample_rate=int(active_env.get("XIAOXIN_TTS_STREAM_SAMPLE_RATE", "24000") or "24000"),
        frame_duration_ms=int(active_env.get("XIAOXIN_TTS_STREAM_FRAME_MS", "60") or "60"),
        max_download_bytes=int(active_env.get("XIAOXIN_TTS_MAX_DOWNLOAD_BYTES", str(4 * 1024 * 1024)) or str(4 * 1024 * 1024)),
    )


def _download_audio_url(url: str, config: TtsAudioStreamConfig) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "hzcu-xiaoxin-tts-stream/1.0"})
    with urllib.request.urlopen(request, timeout=config.download_timeout_seconds) as response:
        content_length = response.headers.get("Content-Length")
        if content_length and int(content_length) > config.max_download_bytes:
            raise TtsAudioStreamError("tts audio download is too large")
        data = response.read(config.max_download_bytes + 1)
    if len(data) > config.max_download_bytes:
        raise TtsAudioStreamError("tts audio download exceeded max bytes")
    return data


def _ffmpeg_to_ogg_opus(audio: bytes, audio_format: str, config: TtsAudioStreamConfig) -> bytes:
    input_format = str(audio_format or "").strip().lower()
    command = [
        config.ffmpeg_path,
        "-hide_banner",
        "-loglevel",
        "error",
    ]
    if input_format:
        command += ["-f", input_format]
    command += [
        "-i",
        "pipe:0",
        "-vn",
        "-ac",
        "1",
        "-ar",
        str(config.target_sample_rate),
        "-c:a",
        "libopus",
        "-application",
        "audio",
        "-frame_duration",
        str(config.frame_duration_ms),
        "-f",
        "ogg",
        "pipe:1",
    ]
    result = subprocess.run(
        command,
        input=audio,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=config.transcode_timeout_seconds,
        check=False,
    )
    if result.returncode != 0:
        raise TtsAudioStreamError("ffmpeg failed to transcode tts audio")
    return result.stdout


def _opus_packets_from_ogg(data: bytes) -> list[bytes]:
    packets: list[bytes] = []
    partial = bytearray()
    offset = 0
    while offset < len(data):
        if data[offset:offset + 4] != b"OggS":
            raise TtsAudioStreamError("invalid ogg page")
        if offset + 27 > len(data):
            raise TtsAudioStreamError("truncated ogg page header")
        page_segments = data[offset + 26]
        segment_table_start = offset + 27
        segment_table_end = segment_table_start + page_segments
        if segment_table_end > len(data):
            raise TtsAudioStreamError("truncated ogg segment table")
        segment_sizes = data[segment_table_start:segment_table_end]
        body_start = segment_table_end
        body_end = body_start + sum(segment_sizes)
        if body_end > len(data):
            raise TtsAudioStreamError("truncated ogg page body")

        cursor = body_start
        for segment_size in segment_sizes:
            partial.extend(data[cursor:cursor + segment_size])
            cursor += segment_size
            if segment_size < 255:
                packet = bytes(partial)
                partial.clear()
                if not packet.startswith(b"OpusHead") and not packet.startswith(b"OpusTags"):
                    packets.append(packet)
        offset = body_end
    return packets


def opus_packets_from_tts_payload(
    payload: Mapping[str, Any],
    config: TtsAudioStreamConfig | None = None,
) -> list[bytes]:
    active_config = config or load_tts_audio_stream_config()
    if not active_config.enabled:
        return []
    audio_url = str(payload.get("audio_url") or "").strip()
    if not audio_url:
        return []
    audio_format = str(payload.get("audio_format") or "").strip().lower()
    try:
        audio = _download_audio_url(audio_url, active_config)
        ogg = _ffmpeg_to_ogg_opus(audio, audio_format, active_config)
        return _opus_packets_from_ogg(ogg)
    except Exception:
        return []
```

- [ ] **Step 4: Document stream env vars**

In `D:\AI_Pet\hzcu_xiaoxin\web\.env.example`, add:

```dotenv
# Binary TTS downlink for ESP firmware.
XIAOXIN_BINARY_TTS_ENABLED=true
XIAOXIN_FFMPEG_PATH=ffmpeg
XIAOXIN_TTS_STREAM_SAMPLE_RATE=24000
XIAOXIN_TTS_STREAM_FRAME_MS=60
XIAOXIN_TTS_MAX_DOWNLOAD_BYTES=4194304
```

- [ ] **Step 5: Run TTS stream tests**

Run:

```powershell
python -m unittest tests.test_tts_audio_stream
```

Expected: PASS.

- [ ] **Step 6: Commit Task 3**

Run from `D:\AI_Pet\hzcu_xiaoxin`:

```powershell
git add web/tts_audio_stream.py web/.env.example web/tests/test_tts_audio_stream.py
git commit -m "feat: convert tts audio to opus stream"
```

---

### Task 4: Server WebSocket TTS Start/Binary/Stop Downlink

**Files:**
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\firmware_protocol.py`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\server.py`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\scripts\check_hardware_voice_loop.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_protocol.py`
- Test: `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`

**Interfaces:**
- Consumes:
  - `tts_audio_stream.opus_packets_from_tts_payload(payload) -> list[bytes]`
  - `firmware_binary.build_audio_frame(payload, protocol_version) -> bytes`
- Produces:
  - `firmware_protocol.tts_start() -> dict[str, str]`
  - WebSocket reply sequence when audio packets exist: `stt`, `llm`, `tts start`, `tts sentence_start`, `bytes...`, `tts stop`.

- [ ] **Step 1: Add failing protocol test for `tts_start`**

In `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_protocol.py`, add:

```python
    def test_tts_start_matches_firmware_state_machine(self):
        self.assertEqual(protocol.tts_start(), {"type": "tts", "state": "start"})
```

- [ ] **Step 2: Run failing protocol test**

Run:

```powershell
python -m unittest tests.test_firmware_protocol.FirmwareProtocolTest.test_tts_start_matches_firmware_state_machine
```

Expected: FAIL with `AttributeError`.

- [ ] **Step 3: Implement `tts_start`**

In `D:\AI_Pet\hzcu_xiaoxin\web\firmware_protocol.py`, add:

```python
def tts_start() -> dict[str, str]:
    return {"type": "tts", "state": "start"}
```

- [ ] **Step 4: Add failing server test for binary TTS downlink sequence**

In `D:\AI_Pet\hzcu_xiaoxin\web\tests\test_firmware_server.py`, add:

```python
    def test_text_turn_streams_tts_binary_frames_for_protocol_3_firmware(self):
        user = self.store.create_user("alice", "secret")
        pairing = self.store.create_pairing_code("device-1", "client-1")
        self.store.bind_pairing_code(pairing["code"], user["user_id"])

        original_reply = server.conversation_core.reply_to_text
        original_tts = server.tts_service.synthesize_speech
        original_stream = server.tts_audio_stream.opus_packets_from_tts_payload
        server.conversation_core.reply_to_text = lambda *args, **kwargs: {
            "reply": "\u6211\u5728",
            "speech": "\u6211\u5728",
            "expression": "smile",
        }
        server.tts_service.synthesize_speech = lambda *args, **kwargs: {
            "audio_url": "https://example.test/tts.mp3",
            "audio_format": "mp3",
            "tts_provider": "fake",
        }
        server.tts_audio_stream.opus_packets_from_tts_payload = lambda payload: [b"opus-a", b"opus-b"]
        try:
            with self.client.websocket_connect(
                "/xiaoxin/v1",
                headers={"Device-Id": "device-1", "Client-Id": "client-1"},
            ) as websocket:
                websocket.send_json({"type": "hello", "version": 3})
                websocket.receive_json()
                websocket.send_json({"type": "text", "text": "\u4f60\u597d"})
                stt = websocket.receive_json()
                llm = websocket.receive_json()
                start = websocket.receive_json()
                sentence = websocket.receive_json()
                audio_a = websocket.receive_bytes()
                audio_b = websocket.receive_bytes()
                stop = websocket.receive_json()
        finally:
            server.conversation_core.reply_to_text = original_reply
            server.tts_service.synthesize_speech = original_tts
            server.tts_audio_stream.opus_packets_from_tts_payload = original_stream

        self.assertEqual(stt, {"type": "stt", "text": "\u4f60\u597d"})
        self.assertEqual(llm, {"type": "llm", "emotion": "smile"})
        self.assertEqual(start, {"type": "tts", "state": "start"})
        self.assertEqual(sentence["type"], "tts")
        self.assertEqual(sentence["state"], "sentence_start")
        self.assertEqual(sentence["text"], "\u6211\u5728")
        self.assertEqual(audio_a, b"\x00\x00\x00\x06opus-a")
        self.assertEqual(audio_b, b"\x00\x00\x00\x06opus-b")
        self.assertEqual(stop, {"type": "tts", "state": "stop"})
```

- [ ] **Step 5: Run failing server downlink test**

Run:

```powershell
python -m unittest tests.test_firmware_server.FirmwareServerWebSocketTest.test_text_turn_streams_tts_binary_frames_for_protocol_3_firmware
```

Expected: FAIL because the server does not send `tts start` or binary frames.

- [ ] **Step 6: Implement server binary TTS downlink**

In `D:\AI_Pet\hzcu_xiaoxin\web\server.py`, add imports:

```python
import tts_audio_stream
```

Add helper inside `firmware_socket`:

```python
        async def send_tts_with_optional_audio(text: str, audio_payload: dict[str, Any]) -> None:
            try:
                opus_packets = await _run_blocking_with_timeout(
                    tts_audio_stream.opus_packets_from_tts_payload,
                    audio_payload,
                    timeout_seconds=app.state.blocking_timeout_seconds,
                )
            except TimeoutError:
                audio_payload = {**audio_payload, "tts_error": "stream_timeout"}
                opus_packets = []
            except Exception:
                audio_payload = {**audio_payload, "tts_error": "stream_failed"}
                opus_packets = []

            if opus_packets:
                await websocket.send_json(protocol.tts_start())
            await websocket.send_json(protocol.tts_sentence_start(text, audio_payload))
            for packet in opus_packets:
                await websocket.send_bytes(
                    firmware_binary.build_audio_frame(packet, client_protocol_version)
                )
            await websocket.send_json(protocol.tts_stop())
```

Replace the existing:

```python
            await websocket.send_json(
                protocol.tts_sentence_start(speech, audio_payload)
            )
            await websocket.send_json(protocol.tts_stop())
```

with:

```python
            await send_tts_with_optional_audio(speech, audio_payload)
```

Do not use this helper for ASR-empty prompts unless they have a real `audio_url`; those prompts may remain text-only.

- [ ] **Step 7: Update manual check script to print binary frame summaries**

In `D:\AI_Pet\hzcu_xiaoxin\web\scripts\check_hardware_voice_loop.py`, replace:

```python
                print(ws.recv())
```

inside the receive loop with:

```python
                message = ws.recv()
                if isinstance(message, bytes):
                    print(f"<binary {len(message)} bytes>")
                else:
                    print(message)
```

- [ ] **Step 8: Run focused server protocol tests**

Run:

```powershell
python -m unittest tests.test_firmware_protocol tests.test_firmware_binary tests.test_tts_audio_stream tests.test_firmware_server
```

Expected: PASS.

- [ ] **Step 9: Commit Task 4**

Run from `D:\AI_Pet\hzcu_xiaoxin`:

```powershell
git add web/firmware_protocol.py web/server.py web/scripts/check_hardware_voice_loop.py web/tests/test_firmware_protocol.py web/tests/test_firmware_server.py
git commit -m "feat: stream tts audio over firmware websocket"
```

---

### Task 5: Firmware Binary Frame Validation

**Files:**
- Modify: `D:\AI_Pet\hzcu_xiaoxin_firmwire\main\protocols\protocol.h`
- Modify: `D:\AI_Pet\hzcu_xiaoxin_firmwire\main\protocols\websocket_protocol.cc`
- Create: `D:\AI_Pet\hzcu_xiaoxin_firmwire\tests\websocket_binary_protocol_test.c`

**Interfaces:**
- Consumes: server protocol 3 binary frames created by `firmware_binary.build_audio_frame`.
- Produces:
  - `static inline bool ParseBinaryProtocol3AudioFrame(const char* data, size_t len, const uint8_t** payload, size_t* payload_size)`

- [ ] **Step 1: Add failing firmware host test for v3 binary parser**

Create `D:\AI_Pet\hzcu_xiaoxin_firmwire\tests\websocket_binary_protocol_test.c`:

```c
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "../main/protocols/protocol.h"

static void test_valid_protocol3_frame(void) {
    const uint8_t frame[] = {0x00, 0x00, 0x00, 0x03, 'a', 'b', 'c'};
    const uint8_t* payload = NULL;
    size_t payload_size = 0;

    assert(ParseBinaryProtocol3AudioFrame((const char*)frame, sizeof(frame), &payload, &payload_size));
    assert(payload_size == 3);
    assert(memcmp(payload, "abc", 3) == 0);
}

static void test_rejects_truncated_header(void) {
    const uint8_t frame[] = {0x00, 0x00, 0x00};
    const uint8_t* payload = NULL;
    size_t payload_size = 0;

    assert(!ParseBinaryProtocol3AudioFrame((const char*)frame, sizeof(frame), &payload, &payload_size));
}

static void test_rejects_payload_length_mismatch(void) {
    const uint8_t frame[] = {0x00, 0x00, 0x00, 0x09, 'a', 'b', 'c'};
    const uint8_t* payload = NULL;
    size_t payload_size = 0;

    assert(!ParseBinaryProtocol3AudioFrame((const char*)frame, sizeof(frame), &payload, &payload_size));
}

static void test_rejects_non_audio_type(void) {
    const uint8_t frame[] = {0x01, 0x00, 0x00, 0x03, 'a', 'b', 'c'};
    const uint8_t* payload = NULL;
    size_t payload_size = 0;

    assert(!ParseBinaryProtocol3AudioFrame((const char*)frame, sizeof(frame), &payload, &payload_size));
}

int main(void) {
    test_valid_protocol3_frame();
    test_rejects_truncated_header();
    test_rejects_payload_length_mismatch();
    test_rejects_non_audio_type();
    puts("websocket_binary_protocol_test passed");
    return 0;
}
```

- [ ] **Step 2: Compile the failing firmware host test**

Run from `D:\AI_Pet\hzcu_xiaoxin_firmwire`:

```powershell
gcc -std=c11 -Itests/stubs -Imain -Imain/protocols tests/websocket_binary_protocol_test.c -o build/websocket_binary_protocol_test.exe
```

Expected: FAIL because `ParseBinaryProtocol3AudioFrame` is not defined.

- [ ] **Step 3: Implement inline parser in `protocol.h`**

In `D:\AI_Pet\hzcu_xiaoxin_firmwire\main\protocols\protocol.h`, after `BinaryProtocol3`, add:

```cpp
static inline bool ParseBinaryProtocol3AudioFrame(
    const char* data,
    size_t len,
    const uint8_t** payload,
    size_t* payload_size
) {
    if (payload == nullptr || payload_size == nullptr) {
        return false;
    }
    *payload = nullptr;
    *payload_size = 0;
    if (data == nullptr || len < sizeof(BinaryProtocol3)) {
        return false;
    }

    const BinaryProtocol3* bp3 = reinterpret_cast<const BinaryProtocol3*>(data);
    if (bp3->type != 0) {
        return false;
    }

    uint16_t size = ntohs(bp3->payload_size);
    if (len != sizeof(BinaryProtocol3) + size) {
        return false;
    }

    *payload = bp3->payload;
    *payload_size = size;
    return true;
}
```

If `ntohs` is not available to C compilation through this header, add `#include <arpa/inet.h>` near the existing includes in `protocol.h`.

- [ ] **Step 4: Use parser helper in `websocket_protocol.cc`**

In `D:\AI_Pet\hzcu_xiaoxin_firmwire\main\protocols\websocket_protocol.cc`, replace the version 3 binary branch:

```cpp
                } else if (version_ == 3) {
                    BinaryProtocol3* bp3 = (BinaryProtocol3*)data;
                    bp3->type = bp3->type;
                    bp3->payload_size = ntohs(bp3->payload_size);
                    auto payload = (uint8_t*)bp3->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>(payload, payload + bp3->payload_size)
                    }));
```

with:

```cpp
                } else if (version_ == 3) {
                    const uint8_t* payload = nullptr;
                    size_t payload_size = 0;
                    if (!ParseBinaryProtocol3AudioFrame(data, len, &payload, &payload_size)) {
                        ESP_LOGW(TAG, "Ignoring malformed protocol 3 audio frame, len=%u", len);
                        return;
                    }
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>(payload, payload + payload_size)
                    }));
```

- [ ] **Step 5: Compile and run firmware host test**

Run:

```powershell
gcc -std=c11 -Itests/stubs -Imain -Imain/protocols tests/websocket_binary_protocol_test.c -o build/websocket_binary_protocol_test.exe
.\build\websocket_binary_protocol_test.exe
```

Expected:

```text
websocket_binary_protocol_test passed
```

- [ ] **Step 6: Build firmware or run the existing compile command**

Run the normal ESP-IDF build command used for this repo:

```powershell
idf.py build
```

Expected: build succeeds. If ESP-IDF is not available in the shell, record the exact missing command error and run the host test from Step 5 as the minimum verification.

- [ ] **Step 7: Commit Task 5**

Run from `D:\AI_Pet\hzcu_xiaoxin_firmwire`:

```powershell
git add main/protocols/protocol.h main/protocols/websocket_protocol.cc tests/websocket_binary_protocol_test.c
git commit -m "fix: validate websocket binary audio frames"
```

---

### Task 6: End-To-End Manual Check And Documentation

**Files:**
- Modify: `D:\AI_Pet\hzcu_xiaoxin\docs\PROJECT_GUIDE.md`
- Modify: `D:\AI_Pet\hzcu_xiaoxin\web\scripts\check_hardware_voice_loop.py`
- Optional Modify: `D:\AI_Pet\hzcu_xiaoxin_firmwire\docs\update.md`

**Interfaces:**
- Consumes: running Xiaoxin service, paired firmware device, ffmpeg on server host.
- Produces: documented manual validation path and script output that distinguishes JSON from binary audio.

- [ ] **Step 1: Add documentation for binary TTS requirements**

In `D:\AI_Pet\hzcu_xiaoxin\docs\PROJECT_GUIDE.md`, add a section:

```markdown
### Hardware Voice Loop With Binary TTS Downlink

The firmware uses WebSocket protocol version 3. Microphone audio is sent as protocol-3 binary frames containing raw Opus payloads. Server TTS is returned as:

1. `{"type":"tts","state":"start"}`
2. `{"type":"tts","state":"sentence_start","text":"...","audio_url":"..."}`
3. one or more protocol-3 binary audio frames
4. `{"type":"tts","state":"stop"}`

The server host must have `ffmpeg` available on `PATH`, or set `XIAOXIN_FFMPEG_PATH` to the executable path. If TTS audio cannot be transcoded, the firmware still receives text and `tts stop`, but no binary audio frames.
```

- [ ] **Step 2: Add manual check command example**

In the same section, add:

```markdown
Run a local server:

```powershell
cd D:\AI_Pet\hzcu_xiaoxin\web
python -m uvicorn server:app --host 127.0.0.1 --port 8765
```

Run the manual WebSocket check with a captured Opus file:

```powershell
$audio = Get-ChildItem .\data\hardware_audio\*.opus | Sort-Object LastWriteTime -Descending | Select-Object -First 1
python .\scripts\check_hardware_voice_loop.py --device-id <device-id> --client-id <client-id> --audio $audio.FullName --max-messages 12
```

Expected output includes `stt`, `llm`, `tts start`, `tts sentence_start`, one or more `<binary N bytes>` lines, and `tts stop`.
```
```

- [ ] **Step 3: Run server full tests**

Run from `D:\AI_Pet\hzcu_xiaoxin\web`:

```powershell
python -m unittest discover tests
```

Expected: all tests pass.

- [ ] **Step 4: Run firmware host test**

Run from `D:\AI_Pet\hzcu_xiaoxin_firmwire`:

```powershell
.\build\websocket_binary_protocol_test.exe
```

Expected:

```text
websocket_binary_protocol_test passed
```

- [ ] **Step 5: Run service smoke**

Run from `D:\AI_Pet\hzcu_xiaoxin\web`:

```powershell
$port=8766
$outLog=Join-Path $env:TEMP 'xiaoxin_uvicorn_binary_tts_out.log'
$errLog=Join-Path $env:TEMP 'xiaoxin_uvicorn_binary_tts_err.log'
$p=Start-Process -FilePath python -ArgumentList @('-m','uvicorn','server:app','--host','127.0.0.1','--port',[string]$port) -WorkingDirectory (Get-Location) -PassThru -WindowStyle Hidden -RedirectStandardOutput $outLog -RedirectStandardError $errLog
try {
  Start-Sleep -Seconds 3
  $health = Invoke-RestMethod -Uri "http://127.0.0.1:$port/health" -TimeoutSec 10
  $ota = Invoke-RestMethod -Uri "http://127.0.0.1:$port/xiaozhi/ota/" -TimeoutSec 10
  [pscustomobject]@{ health = ($health | ConvertTo-Json -Compress); websocket = $ota.websocket.url; version = $ota.websocket.version } | ConvertTo-Json -Compress
} finally {
  if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
```

Expected JSON contains:

```json
{"health":"{\"status\":\"ok\"}","websocket":"ws://127.0.0.1:8766/xiaoxin/v1","version":3}
```

- [ ] **Step 6: Commit Task 6 documentation**

Run from `D:\AI_Pet\hzcu_xiaoxin` if only server docs changed:

```powershell
git add docs/PROJECT_GUIDE.md web/scripts/check_hardware_voice_loop.py
git commit -m "docs: document binary tts hardware check"
```

If firmware docs also changed, run from `D:\AI_Pet\hzcu_xiaoxin_firmwire`:

```powershell
git add docs/update.md
git commit -m "docs: note binary tts websocket playback"
```

---

## Final Verification

- [ ] From `D:\AI_Pet\hzcu_xiaoxin\web`, run:

```powershell
python -m unittest discover tests
```

Expected: all server tests pass.

- [ ] From `D:\AI_Pet\hzcu_xiaoxin_firmwire`, run:

```powershell
.\build\websocket_binary_protocol_test.exe
```

Expected: `websocket_binary_protocol_test passed`.

- [ ] From `D:\AI_Pet\hzcu_xiaoxin_firmwire`, run:

```powershell
idf.py build
```

Expected: firmware build succeeds, or the executor reports that ESP-IDF is not available in the current shell and includes the host-test result.

- [ ] With a real paired device, confirm this runtime sequence in logs:

```text
firmware -> server: hello version 3
server -> firmware: hello with audio_params
firmware -> server: listen start
firmware -> server: protocol-3 binary Opus frames
firmware -> server: listen stop
server -> firmware: stt
server -> firmware: llm
server -> firmware: tts start
server -> firmware: tts sentence_start
server -> firmware: protocol-3 binary Opus frames
server -> firmware: tts stop
```

---

## Self-Review

- Spec coverage:
  - Option 2 server-pushed binary audio is covered by Tasks 3 and 4.
  - Firmware protocol v3 playback compatibility is covered by Task 5.
  - Actual firmware upload wrappers are covered by Task 1.
  - Firmware 16 kHz ASR sample-rate alignment is covered by Task 2.
  - Manual verification and docs are covered by Task 6.
- Placeholder scan:
  - The plan contains no unresolved placeholder instructions or vague test directives.
  - Every code-facing step includes exact snippets or exact commands.
- Type consistency:
  - Server helper names are consistent: `parse_audio_frame`, `build_audio_frame`, `opus_packets_from_tts_payload`, `tts_start`.
  - Firmware helper name is consistent: `ParseBinaryProtocol3AudioFrame`.
  - Protocol version 3 frame layout is consistent across server and firmware tasks.
