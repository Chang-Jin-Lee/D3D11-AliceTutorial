from __future__ import annotations

import argparse
from collections import deque
import os
from pathlib import Path

from PIL import Image

ICON_SOURCE_SIZE = (1254, 1254)
LOGO_SOURCE_SIZE = (1536, 1024)
ICON_CROP = (147, 0, 1107, 960)
LOGO_CROP = (0, 160, 1536, 800)
ICON_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def remove_edge_connected_near_white(image: Image.Image, threshold: int = 248) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    queue: deque[tuple[int, int]] = deque()
    visited: set[tuple[int, int]] = set()

    def is_background(x: int, y: int) -> bool:
        red, green, blue, alpha = pixels[x, y]
        return alpha > 0 and red >= threshold and green >= threshold and blue >= threshold

    for x in range(width):
        queue.append((x, 0))
        queue.append((x, height - 1))
    for y in range(height):
        queue.append((0, y))
        queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        if (x, y) in visited or not is_background(x, y):
            continue
        visited.add((x, y))
        red, green, blue, _ = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        for next_x, next_y in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= next_x < width and 0 <= next_y < height:
                queue.append((next_x, next_y))
    return rgba


def _validated_image(path: Path, expected_size: tuple[int, int]) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(path)
    image = Image.open(path)
    image.load()
    if image.size != expected_size:
        image.close()
        raise ValueError(f"{path} must be {expected_size[0]}x{expected_size[1]}, got {image.size}")
    return image


def generate_brand_assets(
    icon_source: Path,
    logo_source: Path,
    icon_master: Path,
    ico_output: Path,
    logo_output: Path,
) -> None:
    with _validated_image(icon_source, ICON_SOURCE_SIZE) as icon_input:
        cropped_icon = icon_input.crop(ICON_CROP)
        transparent_icon = remove_edge_connected_near_white(cropped_icon)
        master = transparent_icon.resize((1024, 1024), Image.Resampling.LANCZOS)
    with _validated_image(logo_source, LOGO_SOURCE_SIZE) as logo_input:
        banner = logo_input.crop(LOGO_CROP).convert("RGB")

    outputs = (icon_master, ico_output, logo_output)
    for output in outputs:
        output.parent.mkdir(parents=True, exist_ok=True)
    temporary = {output: output.with_name(output.name + ".tmp") for output in outputs}
    try:
        master.save(temporary[icon_master], format="PNG")
        master.save(temporary[ico_output], format="ICO", sizes=[(size, size) for size in ICON_SIZES])
        banner.save(temporary[logo_output], format="PNG", optimize=True)
        for output in outputs:
            os.replace(temporary[output], output)
    finally:
        for temp_path in temporary.values():
            temp_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--icon-source", type=Path, required=True)
    parser.add_argument("--logo-source", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.repo_root.resolve()
    generate_brand_assets(
        args.icon_source,
        args.logo_source,
        root / "Dx11/Resource/Icon/AliceTutorialIcon.png",
        root / "Dx11/Resource/Icon/AliceTutorial.ico",
        root / "docs/media/branding/alice-tutorial-logo.png",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
