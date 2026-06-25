from pathlib import Path


BOARD_SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/"
    "esp32-s3-touch-lcd-1.46.cc"
)


def read_source() -> str:
    return BOARD_SOURCE.read_text(encoding="utf-8")


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


def test_power_latch_is_asserted_before_display_initialization():
    source = read_source()
    constructor = function_body(source, "CustomBoard()")
    statements = [
        line.strip()
        for line in constructor.splitlines()
        if line.strip()
    ]
    early_hold = function_body(source, "void InitializePowerHoldEarly()")

    assert "InitializePowerHoldEarly();" in statements
    assert "RequirePowerOnHold();" not in constructor
    hold_idx = statements.index("InitializePowerHoldEarly();")
    i2c_idx = statements.index("InitializeI2c();")
    assert hold_idx < i2c_idx
    assert "gpio_reset_pin(PWR_BUTTON_GPIO);" in early_hold
    assert "gpio_set_direction(PWR_BUTTON_GPIO, GPIO_MODE_INPUT);" in early_hold
    assert "gpio_set_pull_mode(PWR_BUTTON_GPIO, GPIO_PULLUP_ONLY);" in early_hold
    assert "pwr_ignore_until_release_ = gpio_get_level(PWR_BUTTON_GPIO) == 0;" in early_hold
    assert "gpio_reset_pin(PWR_Control_PIN);" in early_hold
    assert "gpio_set_direction(PWR_Control_PIN, GPIO_MODE_OUTPUT);" in early_hold
    assert "gpio_set_level(PWR_Control_PIN, 1);" in early_hold
    assert early_hold.index("gpio_set_level(PWR_Control_PIN, 1);") > early_hold.index(
        "gpio_set_direction(PWR_Control_PIN, GPIO_MODE_OUTPUT);"
    )
    # Power rail stabilization delay was added
    assert "vTaskDelay(pdMS_TO_TICKS(30));" in early_hold


def test_backlight_is_restored_immediately_after_lcd_initialization():
    source = read_source()
    constructor = function_body(source, "CustomBoard()")
    statements = [
        line.strip()
        for line in constructor.splitlines()
        if line.strip()
    ]

    # Display panel init must precede the FIRST backlight operation
    display_idx = statements.index("InitializeSpd2010Display();")
    backlight_calls = [
        i for i, s in enumerate(statements)
        if "GetBacklight()" in s and i > display_idx
    ]
    assert len(backlight_calls) > 0, "Backlight must be configured after display init"

    # The first backlight call (initialization + brightness set) must be
    # before touch init. Later brightness adjustments at constructor end
    # are fine to come after.
    first_backlight_idx = backlight_calls[0]
    touch_idx = statements.index("InitializeTouch();")
    assert first_backlight_idx < touch_idx, \
        "First backlight init must happen before touch init"


def test_runtime_power_button_handles_press_level_directly():
    source = read_source()
    runtime_reader = function_body(source, "bool ReadPwrButtonPressedForRuntime()")
    pwr_section = source[source.index("// Power Button") : source.index("void InitializeDebugConsole()")]

    assert "bool pwr_ignore_until_release_ = false;" in source
    assert "ReadPwrButtonPressedForRuntime()" in source
    assert "pwr_ignore_until_release_" in runtime_reader
    assert "gpio_get_level(PWR_BUTTON_GPIO)" in runtime_reader
    assert "return !gpio_get_level(PWR_BUTTON_GPIO);" not in pwr_section
    assert "RequestPowerOff();" in pwr_section
