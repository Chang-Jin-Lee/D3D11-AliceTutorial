from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw, IcoImagePlugin

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from generate_app_brand_assets import (  # noqa: E402
    ICON_SIZES,
    generate_brand_assets,
    remove_edge_connected_near_white,
)


class BrandAssetTests(unittest.TestCase):
    def test_only_edge_connected_near_white_becomes_transparent(self):
        image = Image.new("RGB", (8, 8), "white")
        draw = ImageDraw.Draw(image)
        draw.rectangle((2, 2, 5, 5), fill="black")
        draw.point((3, 3), fill="white")

        result = remove_edge_connected_near_white(image)

        self.assertEqual(result.getpixel((0, 0))[3], 0)
        self.assertEqual(result.getpixel((3, 3))[3], 255)

    def test_generation_writes_exact_dimensions_and_ico_frames(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            icon_source = temp_path / "icon.png"
            logo_source = temp_path / "logo.png"
            icon_master = temp_path / "out" / "AliceTutorialIcon.png"
            ico_output = temp_path / "out" / "AliceTutorial.ico"
            logo_output = temp_path / "docs" / "alice-tutorial-logo.png"

            icon = Image.new("RGB", (1254, 1254), "white")
            ImageDraw.Draw(icon).rounded_rectangle((40, 10, 1214, 1214), 100, fill="#7aa9ff")
            icon.save(icon_source)
            logo = Image.new("RGB", (1536, 1024), "#202020")
            ImageDraw.Draw(logo).ellipse((620, 250, 916, 700), fill="white")
            logo.save(logo_source)

            generate_brand_assets(icon_source, logo_source, icon_master, ico_output, logo_output)

            with Image.open(icon_master) as master:
                self.assertEqual(master.size, (1024, 1024))
                self.assertEqual(master.mode, "RGBA")
                self.assertEqual(master.getpixel((0, 0))[3], 0)
            with Image.open(logo_output) as banner:
                self.assertEqual(banner.size, (1536, 640))
            with ico_output.open("rb") as stream:
                sizes = IcoImagePlugin.IcoFile(stream).sizes()
            self.assertEqual(sizes, {(size, size) for size in ICON_SIZES})

    def test_missing_source_leaves_no_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            outputs = [temp_path / "master.png", temp_path / "icon.ico", temp_path / "logo.png"]
            with self.assertRaises(FileNotFoundError):
                generate_brand_assets(temp_path / "missing-icon.png", temp_path / "missing-logo.png", *outputs)
            self.assertTrue(all(not output.exists() for output in outputs))


if __name__ == "__main__":
    unittest.main()
