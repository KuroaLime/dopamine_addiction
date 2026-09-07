import base64
import io
import os
from PIL import Image

indir = r"C:\Users\benev\Documents\GitHub\dopamine_addiction\portfolio_images"
outdir = r"C:\Users\benev\Documents\GitHub\dopamine_addiction\portfolio_images_compressed"
os.makedirs(outdir, exist_ok=True)

# name -> (max_width, quality)
config = {
    "01_title.png": (1600, 76),
    "02_lobby0.png": (1100, 74),
    "03_lobby1.png": (1100, 74),
    "04_seotda_playing.png": (1100, 74),
    "05_seotda_end.png": (1100, 74),
    "06_shop.png": (1100, 74),
    "07_goblin_encounter.png": (1100, 74),
    "08_goblin_reward.png": (1100, 74),
    "09_cover.png": (1600, 78),
}

total_b64 = 0
results = {}

for name, (max_w, quality) in config.items():
    path = os.path.join(indir, name)
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if w > max_w:
        new_h = int(h * (max_w / w))
        im = im.resize((max_w, new_h), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, format="JPEG", quality=quality, optimize=True)
    data = buf.getvalue()
    b64 = base64.b64encode(data).decode("ascii")
    total_b64 += len(b64)
    out_name = os.path.splitext(name)[0] + ".jpg"
    with open(os.path.join(outdir, out_name), "wb") as f:
        f.write(data)
    results[name] = (len(data), im.size)
    print(f"{name}: {im.size} -> {len(data)/1024:.0f} KB (b64 {len(b64)/1024:.0f} KB)")

print(f"TOTAL base64: {total_b64/1024/1024:.2f} MB")
