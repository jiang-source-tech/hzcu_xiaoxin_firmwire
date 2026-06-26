from pathlib import Path


BOARD_SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/"
    "esp32-s3-touch-lcd-1.46.cc"
)
NO_AUDIO_CODEC_SOURCE = Path("main/audio/codecs/no_audio_codec.cc")
NO_AUDIO_CODEC_HEADER = Path("main/audio/codecs/no_audio_codec.h")


def read_source() -> str:
    return BOARD_SOURCE.read_text(encoding="utf-8")


def read_file(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"function body not found: {signature}")


def test_waveshare_1_46_microphone_uses_right_i2s_slot():
    source = read_source()
    body = function_body(source, "virtual AudioCodec* GetAudioCodec() override")

    assert "NoAudioCodecSimplex" in body
    assert "AUDIO_I2S_MIC_GPIO_DIN" in body
    assert "AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT);" not in body
    assert "AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT);" in body


def test_waveshare_1_46_speaker_forces_full_software_output_volume():
    source = read_source()
    body = function_body(source, "virtual AudioCodec* GetAudioCodec() override")

    assert "AUDIO_I2S_SPK_GPIO_DOUT" in body
    assert "audio_codec.SetOutputVolume(100);" in body
    assert body.index("audio_codec.SetOutputVolume(100);") > body.index("NoAudioCodecSimplex audio_codec")


def test_waveshare_1_46_speaker_does_not_apply_output_boost():
    source = read_source()
    body = function_body(source, "virtual AudioCodec* GetAudioCodec() override")

    assert "SetOutputBoost" not in body


def test_no_audio_codec_write_uses_only_output_volume_scaling():
    source = read_file(NO_AUDIO_CODEC_SOURCE)
    header = read_file(NO_AUDIO_CODEC_HEADER)
    body = function_body(source, "int NoAudioCodec::Write(const int16_t* data, int samples)")

    assert "SetOutputBoost" not in header
    assert "output_boost_" not in header
    assert "output_boost_" not in body
    assert "int64_t(data[i]) * volume_factor" in body


def test_waveshare_1_46_microphone_does_not_set_input_gain_for_speaker_volume_issue():
    source = read_source()
    body = function_body(source, "virtual AudioCodec* GetAudioCodec() override")

    assert "SetInputGain" not in body


def test_standard_i2s_microphone_read_keeps_raw_scale_for_speaker_volume_issue():
    source = read_file(NO_AUDIO_CODEC_SOURCE)
    body = function_body(source, "int NoAudioCodec::Read(int16_t* dest, int samples)")

    assert "input_gain_" not in body
    assert "value = (int32_t)(value * input_gain_);" not in body
