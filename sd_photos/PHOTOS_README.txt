ESP32 Bus ETA — 照片規格
========================

尺寸：640×480 px（4:3，全螢幕鋪滿無黑邊）
比例：必須 4:3，16:9 手機照要先裁成 4:3 再縮放，否則會留黑邊
格式：JPEG baseline（非 progressive）
張數：50 張以內
容量：單張 ≤ 200 KB（quality 75-80 即可）
命名：img_001.jpg、img_002.jpg …（依序編號）

放置：SD 卡根目錄 /photos/

Mac 複製後、推出前清掉附屬檔：
    rm -f ./photos/._* ./photos/.DS_Store
