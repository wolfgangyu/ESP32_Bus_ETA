# 架構設計文件

> 對應程式碼：`src/` 正式模組（main / WiFiManager / TDXClient / BusData / TouchManager / WxManager / UIManager / Slideshow / LEDManager / SDManager / LightSensor）
> 硬體腳位唯一事實來源：[docs/ESP32.md](ESP32.md)（本檔不再重述腳位）
> 最後更新：2026-10-23（v1.0.0：觸控整合完成、UI 定版、Splash 顯示 NTP 時間、8 秒分頁、夜間暫停、全螢幕 Slideshow）

---

## 1. 系統概覽

```text
setup()
  隔離 觸控CS=33 / SD CS=5  →  TFT_eSPI init (ILI9341, BL=27)
  WiFiManager.begin(SSID, PASS)  →  輪詢直到連線
  NTP sync (time.nist.gov / time.stdtime.gov.tw)
  TDXClient.begin(CLIENT_ID, SECRET)
  WxManager.fetchAll() (HTTP)
  SD / LDR / UI / Slideshow init
  首抓一次 doFetchAndShow()

loop()
  WiFiManager.loop()                    # 斷線重連
  延遲 3s 初始化觸控 (touch.begin(&tft))
  每 60s: TDXClient → BusData → UI 更新
  每 10min: WxManager (HTTP) 更新氣象/AQI
  Slideshow 自動切換
  夜間暫停檢查 (01:00-05:00)
```

**資料流：**

```text
WiFiManager ──IP──▶ (顯示)
TDXClient.fetchBusETAByName("西新里(新新公園)")  # 一次抓回所有同名牌站牌 ETA
      │  raw JSON
      ▼
BusData.parseTDXResponse(all)
      │  BusInfo[]
      ▼
UIManager.drawScreen()  →  TFT_eSPI 上屏
TouchManager.read()  →  事件 (TAP/HOLD/SWIPE) → 頁面切換/校準
WxManager (HTTP)  →  氣象/AQI → UI 天氣列
Slideshow  →  相片輪播 (全螢幕)
```

---

## 2. 模組職責與介面

### 2.1 WiFiManager (`src/WiFiManager.h/cpp`)

| 方法 | 說明 |
| --- | --- |
| `begin(ssid, password)` | 初始化 STA 模式並開始連線 |
| `loop()` | 放入 `loop()`：檢查狀態、斷線自動重連 |
| `isConnected()` | 連線狀態 |
| `getIP()` | 回傳 IP 字串 |
| `getRSSI()` / `getRetryCount()` | 訊號強度 / 重試次數 |

非阻塞輪詢連線，`MAX_RETRY=10` 指數退避。

### 2.2 TDXClient (`src/TDXClient.h/cpp`)

| 方法 | 說明 |
| --- | --- |
| `begin(clientId, clientSecret)` | 設定 OAuth2 憑證 |
| `fetchBusETA(stopName)` | 舊版保留（未使用） |
| `fetchBusETAByName(stopNameZh)` | 依站名一次抓回所有同名牌站牌 ETA（main 用此支） |
| `isTokenValid()` | token 是否未過期 |

OAuth2 Client Credentials：取 `access_token`（TTL ~24h，過期提前 60s 重發）→ 攜帶 Bearer 呼叫
`GET .../v2/Bus/EstimatedTimeOfArrival/City/Taipei?$format=JSON`。端點細節見 [TDX.md](TDX.md)。

**關鍵：HTTPS client 為類別成員變數**，避免生命週期問題導致崩潰：

```cpp
class TDXClient {
private:
    WiFiClientSecure secureClient;  // 成員變數，生命週期跨越 HTTP 請求
    // ...
};
```

### 2.3 BusData (`src/BusData.h/cpp`)

**資料結構 `BusInfo`：**

```cpp
String routeName;     // 路線編號，如 "203"
String destination;   // 站牌全名
String estimateTime;  // 顯示文字（"約21分後到" / "暫無資料"）
String timeStatus;    // StopStatus 輔助文字
int  stopID;
int  stopStatus;      // TDX StopStatus 數字（0=未知, 3=即將到站...）
int  estimateSecs;    // EstimateTime 秒數（-1 = 暫無資料）
int  direction;       // 0=西向, 1=東向
bool isLowFloor;
```

| 方法 | 說明 |
| --- | --- |
| `parseTDXResponse(json)` | 解析 JSON（清空現有） |
| `getCount(direction)` | 依方向回筆數（0=西, 1=東） |
| `getBusAt(direction, index)` | 取該方向第 N 筆 |
| `getTotalCount()` / `getLastUpdateTime()` | 總筆數 / 最近更新 |

**解析邏輯**：`EstimateTime` 秒數為主排序依據（`-1`=暫無資料排最後），同路線同方向保留最近一班；東西向由 `Direction` 欄位（0=西，1=東）分組。

> **TDX 站牌建模注意**：「西新里(新新公園)」在 TDX 中拆成多支同名牌 StopUID，
> 每支站牌只回報各自那條路線的 ETA。需用 `StopName contains '新新公園'` 一次抓回全量。

### 2.4 WxManager (`src/WxManager.h/cpp`)

| 方法 | 說明 |
| --- | --- |
| `fetchAll()` | 抓取氣象 + AQI (HTTP，避免 HTTPS 崩潰) |
| `getTemp()` / `getHum()` / `getAqi()` / `getWxIconType()` | 取得解析結果 |

**Endpoint（HTTP，免 API Key）：**
- `http://api.open-meteo.com/v1/forecast?...` → 氣溫、濕度、風速、WMO 天氣碼
- `http://air-quality-api.open-meteo.com/v1/air-quality?...` → US AQI

> **注意**：改用 HTTP 避免 `WiFiClientSecure` 生命週期問題導致 `InstrFetchProhibited` 崩潰。

### 2.5 TouchManager (`src/TouchManager.h/cpp`)

| 方法 | 說明 |
| --- | --- |
| `begin(TFT_eSPI* tft)` | 初始化校準參數，`tft->setTouch()` |
| `read()` | 回傳 `TouchEvent` (type, x, y, z) |

**Library**：TFT_eSPI 內建 XPT2046 支援（`-D TOUCH_CS=33`）

**TouchEvent 類型**：`TOUCH_NONE` / `TOUCH_TAP` / `TOUCH_HOLD` / `TOUCH_SWIPE_UP/DOWN/LEFT/RIGHT`

**校準參數**：放在 `include/config_local.h`（5 值：x0, x1, y0, y1, flags=0）

### 2.6 UIManager (`src/UIManager.h/cpp`)

| 頁面 | 說明 |
| --- | --- |
| `PAGE_DASHBOARD` | 天氣列 + 公車列表 (東西向各自分頁，8秒自動翻頁) |
| `PAGE_SLIDESHOW` | 相片全螢幕 |
| `PAGE_CALIBRATION` | 觸控 4 點校準 |

### 2.7 main (`src/main.cpp`)

| 職責 | 說明 |
| --- | --- |
| 開機 | 隔離觸控/SD CS → TFT_eSPI init → 顯示「連線中」 |
| Wi-Fi | `WiFiManager` 輪詢重試，連上顯示 IP |
| NTP | `time.nist.gov` / `time.stdtime.gov.tw`，GMT+8 固定 offset，開機 + 每 1h 重對 |
| 觸控 | **延遲 3 秒** `touch.begin(&tft)` (避免開機不穩定) |
| TDX | 每 60s 抓 ETA → `BusData` 解析 → `UIManager` 更新 |
| WxManager | 每 10min 抓氣象/AQI (HTTP) → UI 天氣列 |
| Slideshow | 自動輪播 (全螢幕) |
| LED | GPIO17 綠燈 PWM：連線中 0.5s 快閃 / 已連線 1s 慢閃 / 離線熄 |
| 觸控事件 | 點擊/左滑切換頁面、長按天氣列校準 |

---

## 3. 設定檔

| 設定 | 定義 | 覆蓋 | 備註 |
| --- | --- | --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | `config.h` | `config_local.h` | 不進 git |
| `TDX_CLIENT_ID` / `TDX_CLIENT_SECRET` | `config.h` | `config_local.h` | 不進 git |
| `TARGET_STOP_NAME` | `config_local.h` | — | 站名「西新里(新新公園)」，`fetchBusETAByName` 用此參數 |
| `REFRESH_INTERVAL_S` | `main.cpp` `static const int` | — | 60 秒；舊 `REFRESH_INTERVAL`（`config.h`）已停用 |
| `ITEMS_PER_PAGE` | `UIManager.h` `static const int` | — | 4 筆/頁 |
| `BUS_PAGE_MS` | `UIManager.h` `static const unsigned long` | — | 8000ms 翻頁間隔 |

`config_local.h` 先 include（若存在），`config.h` 提供預設值。

---

## 4. Boot Loop 根因與解決總結

| 現象 | 根因 | 解決方案 |
| --- | --- | --- |
| 開機白屏重開 | `touch.begin()` 在 `setup()` 過早執行 | **延遲 3 秒**在 `loop()` 中初始化 |
| WiFi 後白屏 | `WiFiClientSecure` 區域變數在 `http.end()` 前析構 | **HTTPS client 宣告為類別成員變數** |
| NTP 後崩潰 | `WiFiClientSecure` 在 WxManager HTTPS 請求中析構 | 改用 **HTTP** (open-meteo 支援) |
| TDX API 崩潰 | 同 `WiFiClientSecure` 生命週期問題 | **client 宣告為 TDXClient 成員變數** |

**核心原則**：任何使用 `WiFiClientSecure` 的 HTTPS 請求，client 必須是類別成員變數，或改用 HTTP 端點。

---

## 5. 記憶體預算（ESP32-D0WD-V3, 320 KB SRAM）

| 區塊 | 預估 | 備註 |
| --- | --- | --- |
| Arduino core + WiFi + TLS | ~80 KB | lwIP + mbedTLS |
| TFT_eSPI frame buffer | ~15 KB | 240×320×2 |
| ArduinoJson parse buffer | ~30-50 KB | 依 TDX 回應大小 |
| BusData 快取 | ~5 KB | 20 筆 × BusInfo |
| 堆疊/其他 | ~20 KB | |
| **可用餘裕** | **~150 KB** | 安全邊際 |

> 全鏈路 Flash 佔 34%（~1.38 MB / 4 MB app 區），`max_app_4MB.csv` partition。

---

## 6. 除錯 / 驗證清單

- [x] TFT_eSPI init 成功（ILI9341 + 隔離觸控/SD CS + BL=27）
- [x] Wi-Fi 連 `No7`、serial 印 IP
- [x] NTP 同步成功 (GMT+8)
- [x] WxManager HTTP 取得氣象/AQI 正常
- [x] TDX HTTPS `fetchBusETAByName` 回非空 JSON、`BusData` 解析
- [x] 觸控初始化延遲 3s 正常、長按校準可進入
- [x] UI 三頁切換、ETA 自動翻頁、Slideshow 輪播
- [x] Splash 顯示 IP + NTP 時間（每 500ms 更新）
- [x] 公車分頁：東/西向各自 8 秒翻頁，>4 筆才啟用
- [x] 夜間暫停：01:00-05:00 不抓 TDX，顯示提示
- [ ] 上機長時程穩定（斷線重連、60s 刷新循環、觸控長時程）

---

## 7. 參考

- [docs/ESP32.md](ESP32.md) — 硬體唯一事實來源
- [TDX.md](TDX.md) — TDX 交通 API 端點與金鑰
- [API_INTEGRATIONS.md](API_INTEGRATIONS.md) — 外部 API 整合總覽（TDX + open-meteo 天氣/AQI）
- [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [ArduinoJson v6](https://arduinojson.org/v6/doc/)
- [TDX API 文件](https://tdx.transportdata.tw/api-service)

---

*文件更新: 2026-10-23 (v1.0.0)*