ESP32 Bus ETA — Photo Slideshow Guidelines
===========================================

Folder: /photos/
Format: JPEG only (baseline, not progressive)
Max file size: 200 KB per image
Max resolution: 240×320 (native), recommended ≤192×192
Aspect ratios: 3:2, 16:9, 4:3, 1:1 (auto-fit, centered)
Naming: img_001.jpg, img_002.jpg ... (sequential, max 999)
Count: Up to 50 images (RAM limited)

⚠️ DO NOT upload phone originals (3-10 MB) — will cause
   decode failure / watchdog reset / sluggish UI.

Resize on PC first:
  macOS:  sips -Z 192 img.jpg --out photos/img_001.jpg
  Linux:  convert img.jpg -resize 192x192 photos/img_001.jpg
  Windows: Use Paint / IrfanView batch resize

Tip: JPEG quality 75-80 gives best size/quality trade-off.