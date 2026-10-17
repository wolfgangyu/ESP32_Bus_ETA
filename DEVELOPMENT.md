# 開發指南（建置 / 燒錄 / 監控 / FAQ）

> 硬體規格請看 [docs/ESP32.md](docs/ESP32.md)；模組架構請看 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。本檔只講怎麼建置與除錯。

---

## 建置指令

```bash
cd /Users/wolfgang/Projects/ESP32_Bus_ETA

# 建置（預設 env：esp32dev，編 main.cpp + 正式模組）
pio run -e esp32dev

# 燒錄
pio run -e esp32dev -t upload

# 監控序列埠（需先釋放 port）
pio device monitor

# 清除建置產物
pio run -e esp32dev -t clean
```

> 序列埠是 `/dev/cu.usbserial-110`。上傳前 port 常被 serial monitor 佔住，先 `kill $(lsof -t /dev/cu.usbserial-110)` 再上傳。

---

## VS Code

1. 開啟專案資料夾，PlatformIO 擴充會自動偵測 `platformio.ini`
2. 底部狀態列：✓ 建置、→ 上傳、📺 序列監控

---

## 必要金鑰與本機參數

- Wi-Fi、TDX API Key 一律填在 `include/config_local.h`（**已 gitignored**，勿上傳）
- `config.h` 是預設值範本，`config_local.h` 覆蓋它
- TDX 金鑰申請與端點說明見 [TDX.md](TDX.md)

**`.env` 與 `config_local.h` 的關係**：`.env`（根目錄，已 gitignore）是金鑰來源記錄檔，`config_local.h` 是實際編入韌體的檔。金鑰變更時先改 `.env` 再同步到 `config_local.h`，避免兩處不一致。

---

## 常見問題

### 螢幕全白 / init 無效

依序檢查（見 `docs/ESP32.md`）：
1. `platformio.ini` build_flags 有 `ILI9341_DRIVER=1`、`TFT_BL=27`、`USER_SETUP_LOADED=1`
2. init 前**觸控 CS=33、SD CS=5 已拉高隔離**（缺了會全白）
3. 背光 27 HIGH
4. strapping：GPIO 12 拉低

### Wi-Fi 連線失敗

- 確認 `config_local.h` 的 SSID / 密碼正確
- 板子只支援 2.4GHz，不支援 5GHz

### TDX 回應空 / 解析 0 筆

- 確認 TDX Client ID / Secret 有效
- 站牌用 `TARGET_STOP_WEST/EAST` 的 stopID（config_local.h）
- serial 印 `[TDX] fetched, total=N`，N=0 時查金鑰或站牌 ID
