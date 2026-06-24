"""Generate a 250x58 grayscale noise texture for frosted-glass surface simulation."""
import numpy as np
from pathlib import Path

W, H = 250, 58
OUT = Path(__file__).resolve().parents[1] / "main" / "boards" / "waveshare" / "esp32-s3-touch-lcd-1.46" / "glass_texture.png"


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
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(str(OUT))
    print(f"Glass texture written to {OUT}  ({W}x{H}, 8-bit grayscale)")


if __name__ == "__main__":
    main()
