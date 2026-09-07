import urllib.request
import os

names = [
    "01_title.png",
    "02_lobby0.png",
    "03_lobby1.png",
    "04_seotda_playing.png",
    "05_seotda_end.png",
    "06_shop.png",
    "07_goblin_encounter.png",
    "08_goblin_reward.png",
    "09_cover.png",
]

outdir = r"C:\Users\benev\Documents\GitHub\dopamine_addiction\portfolio_images"
os.makedirs(outdir, exist_ok=True)

with open("image_urls.txt", encoding="utf-8") as f:
    urls = [line.strip() for line in f if line.strip()]

req_headers = {"User-Agent": "Mozilla/5.0"}

for name, url in zip(names, urls):
    path = os.path.join(outdir, name)
    try:
        req = urllib.request.Request(url, headers=req_headers)
        with urllib.request.urlopen(req, timeout=30) as resp, open(path, "wb") as out:
            out.write(resp.read())
        print("OK", name, os.path.getsize(path))
    except Exception as e:
        print("FAIL", name, e)
