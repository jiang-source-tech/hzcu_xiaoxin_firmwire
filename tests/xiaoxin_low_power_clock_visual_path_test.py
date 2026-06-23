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


def test_power_save_mode_remains_task3_timer_boundary():
    source = read_source()
    power_save_section = source[
        source.index("virtual void SetPowerSaveMode(bool on) override {"):
        source.index("void DispatchPetTrigger", source.index("virtual void SetPowerSaveMode(bool on) override {"))
    ]
    assert "LvglDisplay::SetPowerSaveMode(on);" in power_save_section
    assert "ShowLowPowerClockScreen();" not in power_save_section
    assert "HideLowPowerClockScreen();" not in power_save_section


def test_low_power_clock_refresh_uses_dedicated_timer_with_display_lock():
    source = read_source()
    assert "esp_timer_handle_t low_power_clock_timer_ = nullptr;" in source
    assert "InitializeLowPowerClockRefreshTimer();" in source
    assert ".name = \"low_power_clock\"" in source
    assert "esp_timer_start_periodic(low_power_clock_timer_, 10 * 1000 * 1000)" in source

    timer_refresh_section = source[
        source.index("void RefreshLowPowerClockScreenFromTimer()"):
        source.index("void InitializeLowPowerClockRefreshTimer()")
    ]
    assert "DisplayLockGuard lock(this);" in timer_refresh_section
    assert "if (!low_power_clock_visible_)" in timer_refresh_section
    assert "RefreshLowPowerClockScreenLocked(false);" in timer_refresh_section


def test_low_power_clock_refresh_is_not_in_hot_render_loop():
    source = read_source()
    render_loop_section = source[
        source.index("void RunRenderLoop()"):
        source.index("static void RenderTask", source.index("void RunRenderLoop()"))
    ]
    assert "RefreshLowPowerClockScreenLocked(false);" not in render_loop_section
