"""Generate the frosted-glass texture PNG and LVGL I8 asset for the heads-up banner."""
import numpy as np
from pathlib import Path

W, H = 250, 58
BOARD_DIR = Path(__file__).resolve().parents[1] / "main" / "boards" / "waveshare" / "esp32-s3-touch-lcd-1.46"
PNG_OUT = BOARD_DIR / "glass_texture.png"
C_OUT = BOARD_DIR / "xiaoxin_notification_heads_up_glass_texture.c"


def gaussian_kernel(size: int, sigma: float) -> np.ndarray:
    ax = np.arange(-size // 2 + 1.0, size // 2 + 1.0)
    xx, yy = np.meshgrid(ax, ax)
    kernel = np.exp(-(xx**2 + yy**2) / (2.0 * sigma**2))
    return kernel / kernel.sum()


def convolve2d(img: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    kh, kw = kernel.shape
    pad_h, pad_w = kh // 2, kw // 2
    padded = np.pad(img.astype(np.float32), ((pad_h, pad_h), (pad_w, pad_w)), mode="reflect")
    result = np.zeros_like(img, dtype=np.float32)
    for y in range(img.shape[0]):
        for x in range(img.shape[1]):
            result[y, x] = np.sum(padded[y : y + kh, x : x + kw] * kernel)
    return result


def main() -> None:
    np.random.seed(42)
    noise = np.random.randint(0, 255, (H, W), dtype=np.uint8).astype(np.float32)
    kernel = gaussian_kernel(5, sigma=0.8)
    blurred = convolve2d(noise, kernel)
    amplitude = 10.0
    centered = 128.0 + (blurred - 128.0) * (amplitude / 128.0)
    result = np.clip(centered, 118, 138).astype(np.uint8)

    from PIL import Image

    img = Image.fromarray(result, mode="L")
    BOARD_DIR.mkdir(parents=True, exist_ok=True)
    img.save(str(PNG_OUT))
    write_lvgl_i8_asset(result)
    print(f"Glass texture written to {PNG_OUT}  ({W}x{H}, 8-bit grayscale)")
    print(f"LVGL asset written to {C_OUT}")


def write_lvgl_i8_asset(pixels: np.ndarray) -> None:
    palette = bytearray()
    for value in range(256):
        palette.extend((value, value, value, 0xFF))

    data = bytes(palette) + pixels.astype(np.uint8).tobytes()
    hex_lines = []
    for start in range(0, len(data), 16):
        chunk = data[start : start + 16]
        hex_lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")

    c_source = "\n".join(
        [
            "#include <lvgl.h>",
            "",
            "#ifdef __cplusplus",
            'extern "C" {',
            "#endif",
            "",
            "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
            "#define LV_ATTRIBUTE_MEM_ALIGN",
            "#endif",
            "",
            "#ifndef LV_ATTRIBUTE_IMAGE_XIAOXIN_HEADS_UP_GLASS_TEXTURE",
            "#define LV_ATTRIBUTE_IMAGE_XIAOXIN_HEADS_UP_GLASS_TEXTURE",
            "#endif",
            "",
            "static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_XIAOXIN_HEADS_UP_GLASS_TEXTURE",
            "uint8_t xiaoxin_heads_up_glass_texture_map[] = {",
            *hex_lines,
            "};",
            "",
            "const lv_image_dsc_t xiaoxin_heads_up_glass_texture = {",
            "    .header = {",
            "        .magic = LV_IMAGE_HEADER_MAGIC,",
            "        .cf = LV_COLOR_FORMAT_I8,",
            "        .flags = 0,",
            f"        .w = {W},",
            f"        .h = {H},",
            f"        .stride = {W},",
            "        .reserved_2 = 0,",
            "    },",
            "    .data_size = sizeof(xiaoxin_heads_up_glass_texture_map),",
            "    .data = xiaoxin_heads_up_glass_texture_map,",
            "    .reserved = NULL,",
            "    .reserved_2 = NULL,",
            "};",
            "",
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
        ]
    )
    C_OUT.write_text(c_source, encoding="utf-8")


if __name__ == "__main__":
    main()
