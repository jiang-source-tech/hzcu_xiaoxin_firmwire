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
    assert "lv_obj_t* low_power_clock_time_glow_label_ = nullptr;" in source
    assert "low_power_clock_time_shadow_label_" not in source
    assert "lv_obj_t* low_power_clock_time_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_date_label_ = nullptr;" in source
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
    assert "lv_obj_set_style_arc_opa(low_power_clock_inner_arc_, LowPowerClockOpaPercent(55), LV_PART_INDICATOR);" in source
    assert "low_power_clock_time_glow_label_ = lv_label_create(low_power_clock_layer_);" in source
    assert "low_power_clock_time_shadow_label_" not in source
    assert "lv_obj_align(low_power_clock_time_label_, LV_ALIGN_CENTER, 0, -10);" in source
    assert "const lv_font_t* clock_font = &font_puhui_basic_30_4;" in source
    assert "ConfigureLowPowerTimeLabel(low_power_clock_time_glow_label_, clock_font, 0x75DFFF, LowPowerClockOpaPercent(22), 2, 736, 104, 40);" in source
    assert "ConfigureLowPowerTimeLabel(low_power_clock_time_label_, clock_font, 0xFFFFFF, LV_OPA_COVER, 2, 704, 96, 36);" in source
    assert "RefreshLowPowerTimeLabelLocked(low_power_clock_time_glow_label_, low_power_clock_snapshot_.time_text, 0, -10);" in source
    assert "RefreshLowPowerTimeLabelLocked(low_power_clock_time_label_, low_power_clock_snapshot_.time_text, 0, -10);" in source
    assert "lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, 0);" in source
    assert "lv_obj_set_style_transform_pivot_y(label, lv_obj_get_height(label) / 2, 0);" in source
    assert "lv_obj_align(low_power_clock_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -18);" in source


def test_low_power_clock_uses_orbit_aod_visual_language():
    source = read_source()

    assert "lv_obj_t* low_power_clock_outer_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_inner_arc_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_date_label_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_dot_ = nullptr;" in source
    assert "lv_obj_t* low_power_clock_sync_label_ = nullptr;" in source
    assert "lv_obj_set_style_arc_color(low_power_clock_outer_arc_, lv_color_hex(0x071015), LV_PART_MAIN);" in source
    assert "lv_obj_set_style_arc_opa(low_power_clock_outer_arc_, LowPowerClockOpaPercent(35), LV_PART_MAIN);" in source
    assert "lv_obj_set_style_arc_color(low_power_clock_inner_arc_, lv_color_hex(0x1C6B4A), LV_PART_INDICATOR);" in source
    assert "lv_obj_set_style_arc_opa(low_power_clock_inner_arc_, LowPowerClockOpaPercent(55), LV_PART_INDICATOR);" in source
    assert "lv_obj_align(low_power_clock_date_label_, LV_ALIGN_TOP_MID, 0, 34);" in source
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


def test_low_power_clock_state_includes_date_and_sync_status_without_battery_monitoring():
    source = read_source()
    build_state_section = source[
        source.index("xiaoxin_low_power_clock_state_t BuildLowPowerClockState("):
        source.index("void RefreshLowPowerClockScreenLocked(bool force)")
    ]

    assert "battery_snapshot_" not in build_state_section
    assert "GetTimeSyncStatus()" in build_state_section
    assert "state.month = timeinfo.tm_mon + 1;" in build_state_section
    assert "state.day = timeinfo.tm_mday;" in build_state_section
    assert "state.weekday = timeinfo.tm_wday;" in build_state_section
    assert "state.battery_known" not in build_state_section
    assert "state.battery_percent" not in build_state_section


def test_low_power_clock_refresh_updates_orbit_secondary_labels():
    source = read_source()
    refresh_section = source[
        source.index("void RefreshLowPowerClockScreenLocked(bool force)"):
        source.index("void RefreshLowPowerClockAnimationLocked()")
    ]

    assert "lv_label_set_text(low_power_clock_date_label_, low_power_clock_snapshot_.date_text);" in refresh_section
    assert "low_power_clock_battery_label_" not in refresh_section
    assert "low_power_clock_snapshot_.battery_text" not in refresh_section
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
    assert "esp_timer_start_periodic(low_power_clock_timer_, 500 * 1000)" in source
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
    assert "low_power_clock_time_glow_label_ = lv_label_create(low_power_clock_layer_);" in clock_section
    assert "low_power_clock_time_shadow_label_" not in clock_section
    assert "low_power_clock_time_label_ = lv_label_create(low_power_clock_layer_);" in clock_section
    assert "low_power_clock_date_label_ = lv_label_create(low_power_clock_layer_);" in clock_section
    assert "low_power_clock_sync_label_ = lv_label_create(low_power_clock_layer_);" in clock_section
    assert "low_power_clock_hint_label_ = lv_label_create(low_power_clock_layer_);" in clock_section
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


def test_low_power_clock_snake_moves_with_random_direction_state():
    source = read_source()
    state_section = source[
        source.index("lv_obj_t* low_power_clock_layer_ = nullptr;"):
        source.index("uint8_t settings_item_count_ = 0;")
    ]

    assert "enum class LowPowerSnakeDirection" in source
    assert "LowPowerSnakeCell low_power_clock_snake_body_[k_low_power_snake_max_length] = {};" in state_section
    assert "uint8_t low_power_clock_snake_length_ = k_low_power_snake_initial_length;" in state_section
    assert "LowPowerSnakeDirection low_power_clock_snake_direction_" in state_section
    assert "uint32_t low_power_clock_snake_random_state_" in state_section
    assert "low_power_clock_snake_path_" not in state_section
    assert "low_power_clock_snake_path_count_" not in state_section


def test_low_power_clock_snake_has_fruit_growth_and_capped_length_state():
    source = read_source()
    state_section = source[
        source.index("lv_obj_t* low_power_clock_layer_ = nullptr;"):
        source.index("uint8_t settings_item_count_ = 0;")
    ]

    assert "static constexpr uint8_t k_low_power_snake_initial_length = 9;" in source
    assert "static constexpr uint8_t k_low_power_snake_max_length = 24;" in source
    assert "static constexpr uint8_t k_low_power_snake_fruits_per_growth = 8;" in source
    assert "LowPowerSnakeCell low_power_clock_snake_body_[k_low_power_snake_max_length] = {};" in state_section
    assert "uint8_t low_power_clock_snake_length_ = k_low_power_snake_initial_length;" in state_section
    assert "LowPowerSnakeCell low_power_clock_snake_fruit_" in state_section
    assert "bool low_power_clock_snake_fruit_ready_ = false;" in state_section
    assert "uint8_t low_power_clock_snake_fruit_count_ = 0;" in state_section


def test_low_power_clock_snake_starts_new_game_when_standby_enters():
    source = read_source()
    show_section = source[
        source.index("void ShowLowPowerClockScreen()"):
        source.index("void HideLowPowerClockScreen()")
    ]
    new_game_section = source[
        source.index("void StartNewLowPowerSnakeGameLocked()"):
        source.index("void AdvanceLowPowerSnakeLocked()")
    ]

    assert "StartNewLowPowerSnakeGameLocked();" in show_section
    assert "low_power_clock_snake_length_ = k_low_power_snake_initial_length;" in new_game_section
    assert "low_power_clock_snake_fruit_count_ = 0;" in new_game_section
    assert "low_power_clock_snake_fruit_ready_ = false;" in new_game_section
    assert "ResetLowPowerSnakeLocked(k_low_power_snake_initial_length);" in new_game_section
    assert "GenerateLowPowerSnakeFruitLocked();" in new_game_section


def test_low_power_clock_snake_generates_safe_red_fruit():
    source = read_source()
    fruit_section = source[
        source.index("bool GenerateLowPowerSnakeFruitLocked()"):
        source.index("void StartNewLowPowerSnakeGameLocked()")
    ]
    draw_section = source[
        source.index("void DrawLowPowerSnakeBackground(lv_event_t* e)"):
        source.index("xiaoxin_low_power_clock_state_t BuildLowPowerClockState()")
    ]

    assert "LowPowerSnakeCellInCircle(candidate.col, candidate.row)" in fruit_section
    assert "LowPowerSnakeBodyContains(candidate, low_power_clock_snake_length_)" in fruit_section
    assert "low_power_clock_snake_fruit_ = candidate;" in fruit_section
    assert "low_power_clock_snake_fruit_ready_ = true;" in fruit_section
    assert "low_power_clock_snake_fruit_ready_ = false;" in fruit_section
    assert "DrawLowPowerSnakeFruitLocked(layer);" in draw_section
    assert "lv_color_hex(0xFF4D4D)" in source


def test_low_power_clock_snake_moves_toward_fruit_and_grows_only_until_cap():
    source = read_source()
    advance_section = source[
        source.index("void AdvanceLowPowerSnakeLocked()"):
        source.index("void DrawLowPowerSnakeBackground(lv_event_t* e)")
    ]
    fruit_section = source[
        source.index("void HandleLowPowerSnakeFruitLocked("):
        source.index("void AdvanceLowPowerSnakeLocked()")
    ]
    growth_guard = "if (low_power_clock_snake_length_ < k_low_power_snake_max_length)"
    growth_guard_index = fruit_section.index(growth_guard)
    else_index = fruit_section.index("} else {", growth_guard_index)
    growth_branch = fruit_section[growth_guard_index:else_index]
    cap_branch = fruit_section[else_index:]

    assert "LowPowerSnakeDistanceToFruit(next)" in advance_section
    assert "fruit_distance" in advance_section
    assert "LowPowerSnakeCell previous_tail =" in advance_section
    assert "low_power_clock_snake_body_[low_power_clock_snake_length_ - 1U];" in advance_section
    assert "MoveLowPowerSnakeLocked(safe_direction, &previous_tail);" in advance_section
    assert "MoveLowPowerSnakeLocked(best_direction, &previous_tail);" in advance_section
    assert "HandleLowPowerSnakeFruitLocked(low_power_clock_snake_body_[0], previous_tail);" in advance_section
    assert "LowPowerSnakeCell* previous_tail" in source
    assert "const LowPowerSnakeCell& previous_tail" in fruit_section
    assert "*previous_tail = tail;" in source
    assert "low_power_clock_snake_fruit_count_++;" in fruit_section
    assert "low_power_clock_snake_fruit_count_ >= k_low_power_snake_fruits_per_growth" in fruit_section
    assert growth_guard in fruit_section
    assert "low_power_clock_snake_body_[low_power_clock_snake_length_] = previous_tail;" in growth_branch
    assert "low_power_clock_snake_length_++;" in growth_branch
    assert "low_power_clock_snake_fruit_count_ -= k_low_power_snake_fruits_per_growth;" in growth_branch
    assert "low_power_clock_snake_body_[low_power_clock_snake_length_] = previous_tail;" not in cap_branch
    assert "low_power_clock_snake_length_++;" not in cap_branch
    assert "low_power_clock_snake_fruit_count_ = k_low_power_snake_fruits_per_growth;" in cap_branch
    assert "GenerateLowPowerSnakeFruitLocked();" in fruit_section
    assert "StartNewLowPowerSnakeGameLocked();" not in fruit_section
    assert "ResetLowPowerSnakeLocked(" not in fruit_section


def test_low_power_clock_snake_uses_constrained_random_walk_not_fixed_path():
    source = read_source()

    assert "BuildLowPowerSnakePath(" not in source
    assert "AdvanceLowPowerSnakeLocked()" in source
    assert "ResetLowPowerSnakeLocked(uint8_t target_length)" in source
    assert "NextLowPowerSnakeRandomLocked()" in source
    assert "LowPowerSnakeCanMoveTo(" in source
    assert "LowPowerSnakeNextCell(" in source
    assert "LowPowerSnakeCellInCircle(next.col, next.row)" in source
    assert "LowPowerSnakeReachableSpaceAfterMove(" in source
    assert "best_score" in source
    assert "k_low_power_snake_time_safe" not in source


def test_low_power_clock_snake_can_cross_all_standby_text_areas():
    source = read_source()
    movement_section = source[
        source.index("bool LowPowerSnakeCanMoveTo("):
        source.index("void DrawLowPowerSnakeBackground(lv_event_t* e)")
    ]

    assert "LowPowerSnakeCellInSnakeSafeArea" not in movement_section
    assert "k_low_power_snake_soft_safe" not in source
    assert "k_low_power_snake_bottom_safe_y" not in source


def test_low_power_clock_snake_does_not_restart_during_runtime_movement():
    source = read_source()
    advance_section = source[
        source.index("void AdvanceLowPowerSnakeLocked()"):
        source.index("void DrawLowPowerSnakeBackground(lv_event_t* e)")
    ]

    assert advance_section.count("ResetLowPowerSnakeLocked(low_power_clock_snake_length_);") == 2


def test_low_power_clock_snake_body_uses_gradient_color():
    source = read_source()

    assert "LowPowerSnakeMixColor(" in source
    assert "LowPowerSnakeBodyColor(" in source
    assert "LowPowerSnakeBodyOpa(" in source
    assert "LowPowerSnakeBodyColor(body_index, low_power_clock_snake_length_)" in source
    assert "LowPowerSnakeBodyOpa(body_index, low_power_clock_snake_length_)" in source
    assert "is_head ? lv_color_hex(0x56D364)" not in source
    assert "bright_body ? lv_color_hex(0x2F9E5D) : lv_color_hex(0x24734A)" not in source


def test_low_power_clock_snake_animation_invalidates_background_only():
    source = read_source()
    animation_section = source[
        source.index("void RefreshLowPowerClockAnimationLocked()"):
        source.index("void RefreshLowPowerClockScreenFromTimer()")
    ]

    assert "low_power_clock_snake_tick_++" in animation_section
    assert "lv_obj_invalidate(low_power_clock_snake_bg_);" in animation_section
    assert "lv_obj_create(low_power_clock_layer_)" not in animation_section


def test_low_power_clock_snake_waits_for_redraw_before_next_step():
    source = read_source()
    state_section = source[
        source.index("lv_obj_t* low_power_clock_layer_ = nullptr;"):
        source.index("uint8_t settings_item_count_ = 0;")
    ]
    draw_section = source[
        source.index("void DrawLowPowerSnakeBackground(lv_event_t* e)"):
        source.index("xiaoxin_low_power_clock_state_t BuildLowPowerClockState()")
    ]
    animation_section = source[
        source.index("void RefreshLowPowerClockAnimationLocked()"):
        source.index("void RefreshLowPowerClockScreenFromTimer()")
    ]

    assert "bool low_power_clock_snake_redraw_pending_ = false;" in state_section
    assert "low_power_clock_snake_redraw_pending_ = false;" in draw_section
    assert "if (!low_power_clock_snake_redraw_pending_) {" in animation_section
    assert "AdvanceLowPowerSnakeLocked();" in animation_section
    assert "low_power_clock_snake_redraw_pending_ = true;" in animation_section


def test_low_power_clock_snake_uses_supported_lvgl_opacity_values():
    source = read_source()

    assert "static constexpr lv_opa_t LowPowerClockOpaPercent(uint8_t percent)" in source
    for unsupported_opa in ("LV_OPA_35", "LV_OPA_55", "LV_OPA_75", "LV_OPA_85", "LV_OPA_95"):
        assert unsupported_opa not in source
