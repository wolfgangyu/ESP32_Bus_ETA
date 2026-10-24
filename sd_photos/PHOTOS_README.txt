ESP32 Bus ETA — 照片規格
========================

尺寸：640×480 px（4:3，全螢幕鋪滿無黑邊）
比例：必須 4:3，16:9 手機照要先裁成 4:3 再縮放，否則會留黑邊
格式：JPEG baseline（非 progressive）
      ⚠ macOS「另存為 JPEG」預設常輸出 progressive，
        會導致 Slideshow 色塊 / 失真。務必用以下指令重壓。
      張數：50 張以內
      容量：單張 ≤ 200 KB（quality 78 即可）
      命名：img_001.jpg、img_002.jpg …（依序編號）

重壓指令（選一個）
-------------------

# ImageMagick（推薦，最明確）
convert in.png -interlace none -quality 78 out.jpg

# Sips（macOS 內建，無需安裝）
# 注意：-s progressive 0 是關鍵，不帶這行還是會出 progressive
sips -s format jpeg -s formatOptions 78 in.png --setProperty progressive 0 -o out.jpg

# 一次批量處理所有照片
for f in *.png; do convert "$f" -interlace none -quality 78 "img_$(printf '%03d' $((++n))).jpg"; done

放置：SD 卡根目錄 /photos/

Mac 複製後、推出前清掉附屬檔：
    rm -f ./photos/._* ./photos/.DS_Store
