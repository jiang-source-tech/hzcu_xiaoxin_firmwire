from pathlib import Path


LCD_HEADER = Path("main/display/lcd_display.h")
LCD_SOURCE = Path("main/display/lcd_display.cc")


def read_source(path: Path) -> str:
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


def last_function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
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


def test_bottom_subtitle_messages_are_streamed_for_every_role():
    source = read_source(LCD_SOURCE)
    body = last_function_body(source, "void LcdDisplay::SetChatMessage")

    assert "StartChatMessageStreamLocked(content);" in body
    assert 'strcmp(role, "assistant")' not in body
    assert 'strcmp(role, "system")' not in body
    assert "lv_label_set_text(chat_message_label_, content);" not in body


def test_empty_bottom_subtitle_cancels_stream_and_hides_bar():
    source = read_source(LCD_SOURCE)
    body = last_function_body(source, "void LcdDisplay::SetChatMessage")

    assert "StopChatMessageStreamLocked();" in body
    assert 'lv_label_set_text(chat_message_label_, "");' in body
    assert "lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);" in body
    assert "return;" in body[body.index("StopChatMessageStreamLocked();") :]


def test_bottom_subtitle_stream_uses_lvgl_timer_and_utf8_boundaries():
    header = read_source(LCD_HEADER)
    source = read_source(LCD_SOURCE)
    start_body = function_body(source, "void LcdDisplay::StartChatMessageStreamLocked")
    tick_body = function_body(source, "void LcdDisplay::ChatMessageStreamTimerCallback")
    utf8_body = function_body(source, "size_t LcdDisplay::NextUtf8CharacterEnd")

    assert "lv_timer_t* chat_message_stream_timer_ = nullptr;" in header
    assert "std::string chat_message_stream_text_;" in header
    assert "size_t chat_message_stream_offset_ = 0;" in header
    assert "static void ChatMessageStreamTimerCallback(lv_timer_t* timer);" in header
    assert "static size_t NextUtf8CharacterEnd(const std::string& text, size_t offset);" in header
    assert "lv_timer_create(ChatMessageStreamTimerCallback" in start_body
    assert "lv_timer_reset(chat_message_stream_timer_);" in start_body
    assert "NextUtf8CharacterEnd(self->chat_message_stream_text_" in tick_body
    assert "lv_label_set_text(self->chat_message_label_, frame.c_str());" in tick_body
    assert "lv_timer_pause(self->chat_message_stream_timer_);" in tick_body
    assert "(lead & 0xF0) == 0xE0" in utf8_body
    assert "std::min(text.size(), offset + width)" in utf8_body
