"""Build original MouseSide icons from simple vector shapes; build-time only."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1] / "res"
root.mkdir(exist_ok=True)
for name, color in (("active", "#2769dd"), ("paused", "#7b8798")):
    image = Image.new("RGBA", (256, 256))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((8, 8, 248, 248), radius=58, fill=color)
    draw.rounded_rectangle((78, 45, 178, 211), radius=49, outline="white", width=13)
    draw.line((128, 53, 128, 109), fill="white", width=11)
    draw.rounded_rectangle((122, 69, 134, 93), radius=5, fill="white")
    draw.rounded_rectangle((54, 104, 65, 128), radius=5, fill="white")
    draw.rounded_rectangle((54, 142, 65, 166), radius=5, fill="white")
    image.save(root / f"{name}.ico", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(256,256)])
