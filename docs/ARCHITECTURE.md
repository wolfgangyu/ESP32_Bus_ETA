# 架構設計文件

> 對應程式碼：`src/` 正式模組（main / WiFiManager / TDXClient / BusData）
> 硬體腳位唯一事實來源：[docs/ESP32.md](ESP32.md)（本檔不再重述腳位）
> 最後更新：2026-10-18（TDX.md 移入 docs/；WxManager + open-meteo 氣象/AQI 整合，見 docs/API_INTEGRATIONS.md）

---

## 1. 系統概覽

```text
setup()
  隔離 觸控CS=33 / SD CS=5  →  TFT_eSPI init (ILI9341, BL=27)
  WiFiManager.begin(SSID, PASS)  →  輪詢直到連線
  TDXClient.begin(CLIENT_ID, SECRET)
  首抓一次 doFetchAndShow()

loop()
  WiFiManager.loop()                    # 斷線重連
  每 60s: TDXClient → BusData → 上屏
```

**資料流：**

```text
WiFiManager ──IP──▶ (顯示)
TDXClient.fetchBusETAByName("西新里(新新公園)")  # 一次抓回 22 支站牌
      │  raw JSON
      ▼
BusData.parseTDXResponse(all)
      │  BusInfo[]
      ▼
main::drawScreen()  →  TFT_eSPI 上屏
```

> 觸控（XPT2046）目前**未整合**；`TouchManager` 暫列 `build_src_filter` 之外，之後再加。

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
| `parseTDXResponse(json)` | 解析第一支站牌 JSON（清空現有） |
| `mergeTDXResponse(json)` | 解析並合併第二支站牌（不清空） |
| `getCount(direction)` | 依方向回筆數（0=西, 1=東） |
| `getBusAt(direction, index)` | 取該方向第 N 筆 |
| `getTotalCount()` / `getLastUpdateTime()` | 總筆數 / 最近更新 |

**解析邏輯**：`EstimateTime` 秒數為主排序依據（`-1`=暫無資料排最後），同路線同方向保留最近一班；東西向由 `Direction` 欄位（0=西，1=東）分組，不需再 merge 兩支 stopID。

> **TDX 站牌建模注意**：「西新里(新新公園)」在 TDX 中拆成 22 支同名牌 StopUID，
> 每支站牌只回報各自那條路線的 ETA。查 `StopUID eq 'TPE12726'` 永遠只回 203，
> 需用 `StopName contains '新新公園'` 一次抓回全量。對照表見 [TDX.md](TDX.md)。

### 2.4 main (`src/main.cpp`)

| 職責 | 說明 |
| --- | --- |
| 開機 | 隔離觸控/SD CS → TFT_eSPI init → 顯示「連線中」 |
| Wi-Fi | `WiFiManager` 輪詢重試，連上顯示 IP |
| 拉數 | 每 `REFRESH_INTERVAL_S=60` 秒抓東西向、`BusData` 解析、`drawScreen()` |
| 上屏 | 全英文 ASCII（`LOAD_FONT2`），西向綠 / 東向紫，最多 12 筆 |
| NTP | `time.nist.gov` / `time.stdtime.gov.tw`，GMT+8 固定 offset，開機 + 每 1h 重對 |
| 時鐘 | `clockStr()` 格式 `MM/DD HH:MM`；`clockTick()` 每 500ms 局部重繪表頭右上角，字串沒變且無全清就跳過重繪；`drawScreen()` 全清後設 `clockNeedsRedraw=true` 讓下輪必重印 |
| LED | GPIO17 **綠**燈 PWM 方波（共陰極：LOW=滅、HIGH=亮）：連線中 0.5s 快閃 / 已連線 1s 慢閃 / 離線熄 |

---

## 3. 設定檔

| 設定 | 定義 | 覆蓋 | 備註 |
| --- | --- | --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | `config.h` | `config_local.h` | 不進 git |
| `TDX_CLIENT_ID` / `TDX_CLIENT_SECRET` | `config.h` | `config_local.h` | 不進 git |
| `TARGET_STOP_NAME` | `config_local.h` | — | 站名「西新里(新新公園)」，`fetchBusETAByName` 用此參數 |
| `MAX_BUS_DISPLAY` | `config.h` | — | 顯示參數（舊，實際顯示筆數由 `main.cpp` `drawScreen()` 迴圈上限控制） |
| `REFRESH_INTERVAL_S` | `main.cpp` `static const int` | — | 60 秒；舊 `REFRESH_INTERVAL`（`config.h`）已停用，勿用 |

`config_local.h` 先 include（若存在），`config.h` 提供預設值。

---

## 4. 記憶體預算（ESP32-D0WD-V3, 320 KB SRAM）

| 區塊 | 預估 | 備註 |
| --- | --- | --- |
| Arduino core + WiFi + TLS | ~80 KB | lwIP + mbedTLS |
| TFT_eSPI frame buffer | ~15 KB | 240×320×2 |
| ArduinoJson parse buffer | ~30-50 KB | 依 TDX 回應大小 |
| BusData 快取 | ~5 KB | 20 筆 × BusInfo |
| 堆疊/其他 | ~20 KB | |
| **可用餘裕** | **~150 KB** | 安全邊際 |

> 全鏈路 Flash 佔 76%（~996 KB / 4 MB 中 app 區），TDX 兩支 JSON 併存時注意 heap。

---

## 5. 除錯 / 驗證清單

- [x] TFT_eSPI init 成功（ILI9341 + 隔離觸控/SD CS + BL=27）
- [x] Wi-Fi 連 `No7`、serial 印 IP
- [x] TDX `fetchBusETAByStopID` 回非空 JSON、`BusData` 解析出新新公園車次
- [x] `main.cpp` 全模組編譯並上屏顯示車次
- [ ] 觸控整合（XPT2046，`TouchManager` 回 `build_src_filter`）
- [ ] 上機長時程穩定（斷線重連、60s 刷新循環）

---

## 6. 參考

- [docs/ESP32.md](ESP32.md) — 硬體唯一事實來源
- [TDX.md](TDX.md) — TDX 交通 API 端點與金鑰
- [API_INTEGRATIONS.md](API_INTEGRATIONS.md) — 外部 API 整合總覽（TDX + open-meteo 天氣/AQI）
- [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [ArduinoJson v6](https://arduinojson.org/v6/doc/)
- [TDX API 文件](https://tdx.transportdata.tw/api-service)
