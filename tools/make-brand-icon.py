"""Convert the supplied square logo to an ICO without altering its artwork."""
from pathlib import Path
from PIL import Image

brand = Path(__file__).resolve().parents[1] / "assets/brand"
image = Image.open(brand / "TeamHJD-Logo.png").convert("RGBA")
assert image.width == image.height, "Preserve the brand aspect ratio"
image.save(brand / "TeamHJD.ico", format="ICO", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
