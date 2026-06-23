from pathlib import Path

SOURCE = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")


def read_source():
    return SOURCE.read_text(encoding="utf-8")


def test_low_power_clock_objects_exist_and_are_hidden_by_default():
    source = read_source()
    assert "lv_obj_t* low_power_clock_layer_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_icon_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_time_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_hint_label_ = nullptr;" in source
    assert "lv_obj_add_flag(low_power_clock_layer_, LV_OBJ_FLAG_HIDDEN);" in source


def test_low_power_clock_uses_icon_left_time_right_layout():
    source = read_source()
    assert "lv_obj_align(low_power_clock_icon_label_, LV_ALIGN_TOP_MID" in source
    assert "lv_obj_align_to(low_power_clock_time_label_, low_power_clock_icon_label_, LV_ALIGN_OUT_RIGHT_MID" in source
    assert "lv_obj_set_style_text_font(low_power_clock_icon_label_, icon_font, 0);" in source
    assert "lv_label_set_text(low_power_clock_hint_label_, low_power_clock_snapshot_.hint_text);" in source
    assert "LV_ALIGN_BOTTOM_MID" in source


def test_low_power_clock_enters_with_dim_backlight_and_exits_with_restore():
    source = read_source()
    assert "ShowLowPowerClockScreen()" in source
    assert "HideLowPowerClockScreen()" in source
    assert "backlight->SetBrightness(low_power_clock_snapshot_.brightness_percent, false);" in source
    assert "backlight->RestoreBrightness();" in source


def test_power_button_still_wakes_power_save_timer():
    source = read_source()
    power_section = source[source.index("// Power Button"):]
    assert "self->WakePowerSaveTimer();" in power_section
