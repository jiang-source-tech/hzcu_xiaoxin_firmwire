from pathlib import Path

SOURCE = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")
CMAKE_SOURCE = Path("main/CMakeLists.txt")


def read_source():
    return SOURCE.read_text(encoding="utf-8")


def test_low_power_clock_model_is_compiled_into_main_component():
    cmake = CMAKE_SOURCE.read_text(encoding="utf-8")

    assert "xiaoxin_low_power_clock_model.c" in cmake


def test_low_power_clock_objects_exist_and_are_hidden_by_default():
    source = read_source()
    assert "lv_obj_t* low_power_clock_layer_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_outer_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_inner_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_time_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_date_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_battery_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_dot_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_hint_label_ = nullptr;" in source
    assert "lv_obj_add_flag(low_power_clock_layer_, LV_OBJ_FLAG_HIDDEN);" in source


def test_low_power_clock_uses_large_center_time_and_arc_layout():
    source = read_source()
    assert "LV_FONT_DECLARE(font_puhui_basic_30_4);" in source
    assert "lv_obj_t* low_power_clock_inner_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_outer_arc_ = nullptr;" in source
    assert "lv_arc_create(low_power_clock_layer_)" in source
    assert "lv_obj_set_size(low_power_clock_outer_arc_, DISPLAY_WIDTH - 10, DISPLAY_HEIGHT - 10);" in source
    assert "lv_obj_set_size(low_power_clock_inner_arc_, DISPLAY_WIDTH - 28, DISPLAY_HEIGHT - 28);" in source
    assert "lv_arc_set_bg_angles(low_power_clock_inner_arc_, 0, 360);" in source
    assert "lv_arc_set_angles(low_power_clock_inner_arc_, 0, XIAOXIN_LOW_POWER_CLOCK_ARC_SPAN_DEGREES);" in source
    assert "lv_obj_remove_style(low_power_clock_inner_arc_, NULL, LV_PART_KNOB);" in source
    assert "lv_obj_set_style_arc_color(low_power_clock_inner_arc_, lv_color_hex(0x1C6B4A), LV_PART_INDICATOR);" in source
    assert "lv_obj_set_style_arc_opa(low_power_clock_inner_arc_, LV_OPA_55, LV_PART_INDICATOR);" in source
    assert "lv_obj_align(low_power_clock_time_label_, LV_ALIGN_CENTER, 0, -10);" in source
    assert "const lv_font_t* clock_font = &font_puhui_basic_30_4;" in source
    assert "lv_obj_set_style_text_font(low_power_clock_time_label_, clock_font, 0);" in source
    assert "lv_obj_set_style_transform_scale(low_power_clock_time_label_, 384, 0);" in source
    assert "lv_obj_set_style_transform_width(low_power_clock_time_label_, 56, 0);" in source
    assert "lv_obj_set_style_transform_height(low_power_clock_time_label_, 20, 0);" in source
    assert "lv_obj_set_style_transform_pivot_x(low_power_clock_time_label_, lv_obj_get_width(low_power_clock_time_label_) / 2, 0);" in source
    assert "lv_obj_set_style_transform_pivot_y(low_power_clock_time_label_, lv_obj_get_height(low_power_clock_time_label_) / 2, 0);" in source
    assert "lv_obj_align(low_power_clock_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -18);" in source


def test_low_power_clock_uses_orbit_aod_visual_language():
    source = read_source()

    assert "lv_obj_t* low_power_clock_outer_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_inner_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_date_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_battery_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_dot_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_label_ = nullptr;" in source
    assert "lv_obj_set_style_arc_color(low_power_clock_outer_arc_, lv_color_hex(0x071015), LV_PART_MAIN);" in source
    assert "lv_obj_set_style_arc_opa(low_power_clock_outer_arc_, LV_OPA_35, LV_PART_MAIN);" in source
    assert "lv_obj_set_style_arc_color(low_power_clock_inner_arc_, lv_color_hex(0x1C6B4A), LV_PART_INDICATOR);" in source
    assert "lv_obj_set_style_arc_opa(low_power_clock_inner_arc_, LV_OPA_55, LV_PART_INDICATOR);" in source
    assert "lv_obj_align(low_power_clock_date_label_, LV_ALIGN_TOP_MID, 0, 34);" in source
    assert "lv_obj_align(low_power_clock_battery_label_, LV_ALIGN_BOTTOM_LEFT, 22, -20);" in source
    assert "lv_obj_align(low_power_clock_sync_label_, LV_ALIGN_BOTTOM_RIGHT, -22, -20);" in source


def test_low_power_clock_hint_keeps_smaller_theme_font():
    source = read_source()
    init_section = source[
        source.index("void InitializeLowPowerClockLayerLocked()"):
        source.index("void InitializeCardPagerLayer()", source.index("void InitializeLowPowerClockLayerLocked()"))
    ]

    assert "const lv_font_t* hint_font = lvgl_theme != nullptr && lvgl_theme->text_font() != nullptr" in init_section
    assert "lv_obj_set_style_text_font(low_power_clock_hint_label_, hint_font, 0);" in init_section
    assert "lv_obj_set_style_text_font(low_power_clock_hint_label_, clock_font, 0);" not in init_section


def test_low_power_clock_state_includes_date_battery_and_sync_status():
    source = read_source()
    build_state_section = source[
        source.index("xiaoxin_low_power_clock_state_t BuildLowPowerClockState("):
        source.index("void RefreshLowPowerClockScreenLocked(bool force)")
    ]

    assert "battery_snapshot_" in build_state_section
    assert "GetTimeSyncStatus()" in build_state_section
    assert "state.month = timeinfo.tm_mon + 1;" in build_state_section
    assert "state.day = timeinfo.tm_mday;" in build_state_section
    assert "state.weekday = timeinfo.tm_wday;" in build_state_section
    assert "state.battery_percent = battery_snapshot_.estimated_percent;" in build_state_section


def test_low_power_clock_refresh_updates_orbit_secondary_labels():
    source = read_source()
    refresh_section = source[
        source.index("void RefreshLowPowerClockScreenLocked(bool force)"):
        source.index("void RefreshLowPowerClockAnimationLocked()")
    ]

    assert "lv_label_set_text(low_power_clock_date_label_, low_power_clock_snapshot_.date_text);" in refresh_section
    assert "lv_label_set_text(low_power_clock_battery_label_, low_power_clock_snapshot_.battery_text);" in refresh_section
    assert "lv_label_set_text(low_power_clock_sync_label_, low_power_clock_snapshot_.sync_text);" in refresh_section
    assert "lv_obj_set_style_bg_color(low_power_clock_sync_dot_, lv_color_hex(low_power_clock_snapshot_.sync_color_hex), 0);" in refresh_section


def test_low_power_clock_no_longer_uses_small_icon_left_layout():
    source = read_source()
    assert "lv_obj_t* low_power_clock_icon_label_ = nullptr;" not in source
    assert "lv_obj_align_to(low_power_clock_time_label_, low_power_clock_icon_label_, LV_ALIGN_OUT_RIGHT_MID" not in source
    assert "lv_label_set_text(low_power_clock_icon_label_" not in source


def test_low_power_clock_enters_with_dim_backlight_and_exits_with_restore():
    source = read_source()
    assert "ShowLowPowerClockScreen()" in source
    assert "HideLowPowerClockScreen()" in source
    assert "StartLowPowerClockRefreshTimer();" in source
    assert "StopLowPowerClockRefreshTimer();" in source
    assert "backlight->SetBrightness(low_power_clock_snapshot_.brightness_percent, false);" in source
    assert "backlight->RestoreBrightness();" in source


def test_power_button_still_wakes_power_save_timer():
    source = read_source()
    power_section = source[source.index("// Power Button"):]
    assert "self->WakePowerSaveTimer();" in power_section


def test_power_save_timer_callbacks_show_and_hide_clock_screen():
    source = read_source()
    assert "display->ShowLowPowerClockScreen();" in source
    assert "display->HideLowPowerClockScreen();" in source
    assert "GetDisplay()->SetPowerSaveMode(true);" not in source
    assert "GetDisplay()->SetPowerSaveMode(false);" not in source


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
    assert "void StartLowPowerClockRefreshTimer()" in source
    assert "void StopLowPowerClockRefreshTimer()" in source
    assert "esp_timer_start_periodic(low_power_clock_timer_, 1000 * 1000)" in source
    assert "esp_timer_stop(low_power_clock_timer_)" in source

    timer_refresh_section = source[
        source.index("void RefreshLowPowerClockScreenFromTimer()"):
        source.index("void InitializeLowPowerClockRefreshTimer()")
    ]
    assert "DisplayLockGuard lock(this);" in timer_refresh_section
    assert "if (!low_power_clock_visible_)" in timer_refresh_section
    assert "RefreshLowPowerClockAnimationLocked();" in timer_refresh_section
    assert "RefreshLowPowerClockScreenLocked(false);" in timer_refresh_section

    animation_section = source[
        source.index("void RefreshLowPowerClockAnimationLocked()"):
        source.index("void RefreshLowPowerClockScreenFromTimer()")
    ]
    assert "lv_arc_set_rotation(low_power_clock_inner_arc_, start);" in animation_section

    init_section = source[
        source.index("void InitializeLowPowerClockRefreshTimer()"):
        source.index("void StartLowPowerClockRefreshTimer()")
    ]
    assert "esp_timer_start_periodic" not in init_section


def test_low_power_clock_orbit_animation_updates_two_rings():
    source = read_source()
    animation_section = source[
        source.index("void RefreshLowPowerClockAnimationLocked()"):
        source.index("void RefreshLowPowerClockScreenFromTimer()")
    ]

    assert "lv_arc_set_rotation(low_power_clock_inner_arc_, start);" in animation_section
    assert "lv_arc_set_rotation(low_power_clock_outer_arc_, (start + 180) % 360);" in animation_section
    assert "lv_obj_set_style_opa(low_power_clock_sync_dot_, dot_opa, 0);" in animation_section


def test_backlight_transition_timer_is_not_restarted_while_active():
    source = Path("main/boards/common/backlight.cc").read_text(encoding="utf-8")
    assert "esp_timer_is_active(transition_timer_)" in source
    assert "if (!esp_timer_is_active(transition_timer_))" in source


def test_low_power_clock_refresh_is_not_in_hot_render_loop():
    source = read_source()
    render_loop_section = source[
        source.index("void RunRenderLoop()"):
        source.index("static void RenderTask", source.index("void RunRenderLoop()"))
    ]
    assert "RefreshLowPowerClockScreenLocked(false);" not in render_loop_section


def test_low_power_clock_snake_background_uses_single_drawn_object():
    source = read_source()
    clock_section = source[
        source.index("void InitializeLowPowerClockLayerLocked()"):
        source.index("void InitializeNotificationHeadsUpLayerLocked()", source.index("void InitializeLowPowerClockLayerLocked()"))
    ]
    create_call_lines = [
        line.strip()
        for line in source.splitlines()
        if "lv_obj_create(low_power_clock_layer_)" in line
    ]

    assert "lv_obj_t* low_power_clock_snake_bg_ = nullptr;" in source
    assert "InitializeLowPowerSnakeBackgroundLocked();" in clock_section
    assert create_call_lines == [
        "low_power_clock_sync_dot_ = lv_obj_create(low_power_clock_layer_);",
        "low_power_clock_snake_bg_ = lv_obj_create(low_power_clock_layer_);",
    ]
    assert clock_section.count("lv_arc_create(low_power_clock_layer_)") == 2
    assert clock_section.count("lv_label_create(low_power_clock_layer_)") == 5
    assert clock_section.count("lv_obj_create(screen)") == 1
    assert "lv_obj_add_event_cb(low_power_clock_snake_bg_, LowPowerSnakeDrawEvent, LV_EVENT_DRAW_MAIN, this);" in source
    assert "lv_event_get_layer(e)" in source
    assert "lv_draw_rect(" in source
    assert "lv_draw_rect_dsc_init(" in source
    assert "lv_canvas_create" not in source
    assert "low_power_clock_snake_cells_" not in source


def test_low_power_clock_snake_background_is_created_before_foreground_labels():
    source = read_source()
    init_section = source[
        source.index("void InitializeLowPowerClockLayerLocked()"):
        source.index("void InitializeCardPagerLayer()", source.index("void InitializeLowPowerClockLayerLocked()"))
    ]

    assert init_section.index("InitializeLowPowerSnakeBackgroundLocked();") < init_section.index("low_power_clock_outer_arc_ = lv_arc_create(low_power_clock_layer_);")
    assert init_section.index("InitializeLowPowerSnakeBackgroundLocked();") < init_section.index("low_power_clock_time_label_ = lv_label_create(low_power_clock_layer_);")


def test_low_power_clock_snake_path_clips_circle_and_text_safe_areas():
    source = read_source()
    path_start = source.index("static uint16_t BuildLowPowerSnakePath(")
    fallback_comment = "    // If typewriter jumps around the center read poorly on hardware, use a concentric ring path.\n"
    path_end = source.index("    return count;\n}", source.index(fallback_comment, path_start)) + len("    return count;\n}")
    path_section = source[path_start:path_end]

    assert "BuildLowPowerSnakePath(" in path_section
    assert "LowPowerSnakeCellInCircle(col, row)" in path_section
    assert "LowPowerSnakeCellInSnakeSafeArea(col, row)" in path_section


def test_low_power_clock_snake_animation_invalidates_background_only():
    source = read_source()
    animation_section = source[
        source.index("void RefreshLowPowerClockAnimationLocked()"):
        source.index("void RefreshLowPowerClockScreenFromTimer()")
    ]

    assert "low_power_clock_snake_tick_++" in animation_section
    assert "lv_obj_invalidate(low_power_clock_snake_bg_);" in animation_section
    assert "lv_obj_create(low_power_clock_layer_)" not in animation_section
