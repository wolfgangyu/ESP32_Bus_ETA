# 台灣設計／製造的 ESP32 替代方案盤點

> 盤點日期：2026-10-25  
> 目標：評估是否能以台灣研發或製造的 MCU／開發板，替換本專案 HW-950 的 ESP32，維持 TDX HTTPS 連線、ILI9341 SPI 螢幕、觸控、SD 卡與照片輪播。

## 結論

目前找到最接近 ESP32 整合度的選項是 **Realtek Ameba AMB21（RTL8722DM）**。Realtek 是總部位於新竹的台灣 IC 設計公司，官方將 AMB21 列為 Realtek 設計的開發板。AMB21 具 Wi-Fi、BLE、512 KB SRAM、4 MB PSRAM、2 MB Flash，官方 Arduino SDK 也有 HTTPS 與 ILI9341 範例。

但這只足以支持「台灣公司設計／開發」，**不能證明 RTL8722DM 晶圓在台灣製造，亦不能證明封裝或組裝地**。此外，AMB21 不是 HW-950 的直接替換板；預期要重新接線並移植程式。目前 PlatformIO 官方支援平台清單和 Registry 都沒有找到 Realtek Ameba 平台，官方開發流程以 Arduino IDE 為主。

若要求是「台灣設計、而且能證明這顆 SoC 在台灣製造」，本次查到的官方資料不足以確認有一款整合 Wi-Fi/BLE、效能和開發環境都與 ESP32 相近的單晶片方案。比較務實的候選是先用 AMB21 做原型驗證；若台灣晶圓製造是硬性要求，應先向晶片／模組供應商取得特定型號與批次的產地或製造聲明。

## 候選比較

| 候選 | 台灣關聯與規格 | 對本專案的適合度 |
| --- | --- | --- |
| **Realtek AMB21／RTL8722DM** | Realtek 設計的開發板；雙核心 KM4、KM0；512 KB SRAM、4 MB PSRAM、2 MB Flash；Wi-Fi、BLE 5.0；官方列有 SD Host 與 LCDC。 | **最接近 ESP32 整合用途。** 官方有 HTTPS 與 ILI9341 SPI 範例，適合先驗證。需自行配線，且現有 PlatformIO 專案與函式庫不能假設可直接移植。晶圓製造地未能確認。 |
| **Realtek AMB25／RTL8720DF** | Realtek 設計板；512 KB SRAM、4 MB Flash；雙頻 Wi-Fi 4、BLE 5.0；最多 19 GPIO、2 組 SPI。 | 有 Wi-Fi 與 Arduino SDK，但記憶體和 GPIO 比 AMB21 少；必須實際排好螢幕、觸控、SD 的腳位。製造地未能確認。 |
| **Realtek BW16／RTL8720DN** | Ameba 官方頁標明由 Ai-Thinker 設計，不是 Realtek 設計板；512 KB SRAM、2 MB Flash（可至 4 MB）；Wi-Fi、BLE 5.0；最多 13 GPIO、1 組 SPI。 | 可做 IoT 原型，但對 TFT、觸控、SD 等多週邊較受限；板卡本身也不符合「台灣設計」的嚴格定義。製造地未能確認。 |
| **Nuvoton M487 系列** | 台灣廠商 Nuvoton；Arm Cortex-M4F，最高 192 MHz；最高 2.5 MB Flash、160 KB SRAM；有 SPI、SD Host、LCD 平行介面、乙太網路與硬體加密。Nuvoton 表示其擁有 6 吋晶圓廠。 | 可做台灣 MCU 方案的基礎，但官方 M487 規格主打乙太網路，未列 Wi-Fi／BLE；需要額外無線模組及整合工作。Nuvoton 有晶圓廠，不等同已證明 M487 特定料號由台灣廠製造。 |

## 與目前硬體及程式的差異

- 目前 HW-950 使用 ESP32-D0WD-V3、ILI9341 240 × 320 SPI 螢幕；`platformio.ini` 將 SPI 設為 20 MHz。TFT、觸控和 SD 卡已經過腳位與匯流排整合。
- AMB21 可使用 SPI ILI9341 顯示器，官方也提供 HTTPS 範例；這些範例代表介面能力可行，不代表本專案現有程式碼可以不修改地編譯。
- 開始移植前應先分別驗證：Wi-Fi HTTPS（TDX OAuth/API）、ILI9341 顯示、XPT2046 觸控、microSD SPI、JPEG 解碼與中文字型。特別是觸控與 SD 卡的控制器／函式庫相容性，不應只依「有 SPI」就視為已解決。
- 全螢幕 RGB565 framebuffer 約需 150 KiB。AMB21 的 SRAM 為 512 KB，另有 4 MB PSRAM；但記憶體數字不直接代表所有函式庫都支援 PSRAM，也不保證現有圖片輪播流程可以直接使用。
- PlatformIO 官方文件列出的平台與 Registry 查詢未找到 Realtek Ameba。若不自行維護 PlatformIO 自訂平台，應預期改用 Realtek 官方 Arduino IDE 工具鏈。

## 建議

1. **若優先考慮台灣 IC 設計與接近 ESP32 的整合度：** 用 AMB21／RTL8722DM 做獨立原型，不先拆現有 HW-950。先跑通 HTTPS 與 ILI9341，再接觸控和 SD，最後才移植 TDX、天氣、UI 與 Slideshow。
2. **若 PlatformIO 與現有程式碼可維護性優先：** 留在 ESP32，並針對下一版硬體調查晶片、模組、PCB 與組裝供應鏈。僅更換 MCU 會帶來工具鏈和周邊函式庫的移植成本。
3. **若「台灣製造」是採購硬條件：** 向供應商確認特定料號的晶圓廠、封裝廠、模組產地與組裝地；不能只依品牌總部或公司自有晶圓廠推定。

## 官方來源

- [Realtek：公司簡介](https://www.realtek.com/Article/Index?menu_id=285&lang=zh-TW)（說明公司位於新竹，定位為專業 IC 設計公司）
- [Realtek：公司據點](https://www.realtek.com/Article/Index?menu_id=417&lang=zh-TW)（總部地址）
- [AmebaD 官方開發板與規格](https://www.amebaiot.com/en/amebad/)（AMB21、AMB25、BW16 等板卡設計歸屬、規格與官方購買入口）
- [AmebaD 官方 HTTPS 範例](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_d/amb21/Example_Guides/WiFi/WiFi%20-%20Set%20up%20SSL%20Client%20for%20HTTPS%20Communication.html)
- [AmebaD 官方 ILI9341 SPI 範例](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_d/amb21/Example_Guides/SPI/SPI%20-%20Show%20PM2.5%20Concentration%20on%20ILI9341%20TFT%20LCD.html)
- [Ameba Arduino SDK 發布紀錄](https://github.com/Ameba-AIoT/ameba-arduino-d/releases)（包含 TFT_eSPI 支援與 BW16 SPI SD 範例等更新）
- [PlatformIO 官方支援平台清單](https://docs.platformio.org/en/latest/platforms/index.html)
- [PlatformIO Registry：Realtek Ameba 平台查詢](https://registry.platformio.org/search?t=platform&q=realtek%20ameba)（查詢時沒有找到套件）
- [Nuvoton：M487 Ethernet 系列](https://www.nuvoton.com/products/microcontrollers/arm-cortex-m4-mcus/m487-ethernet-series/)（官方規格）
- [Nuvoton：公司簡介](https://www.nuvoton.com/about-nuvoton/company-profile/overview/)（公司背景與自有 6 吋晶圓廠資訊；不代表 M487 特定型號的製造地）
