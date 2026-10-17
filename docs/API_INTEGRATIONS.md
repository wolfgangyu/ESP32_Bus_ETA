# 外部 API 整合紀錄（TDX 交通 + open-meteo 氣象）

最後更新：2026-10-18

## TDX 交通資料

TDX = Transport Data Exchange（交通部交通資訊交換平台），只有交通數據，跟天氣無關。

### 帳號與金鑰

- 基礎會員已申請，免費會員每月虛擬點數 3 點
- 虛擬點數說明：[tdx.transportdata.tw/pricing](https://tdx.transportdata.tw/pricing)
- 金鑰存放位置：本機 `.env` 檔（gitignored），已同步到 `include/config_local.h`（gitignored）
- **切勿將金鑰上傳至 GitHub**

### 可用端點（實測 2026-10-10）

### 認證

```bash
POST https://tdx.transportdata.tw/auth/realms/TDXConnect/protocol/openid-connect/token
Authorization: Basic base64(client_id:client_secret)
Content-Type: application/x-www-form-urlencoded
Body: grant_type=client_credentials
# → 回傳 access_token（JWT，有效期 ~24h）
```

### 站牌名稱查詢

```bash
GET https://tdx.transportdata.tw/api/basic/v2/Bus/Stop/City/Taipei
  ?$format=JSON
  &$filter=contains(StopName/Zh_tw,'新公園')
  &$top=30
Authorization: Bearer <token>
```

- `v2/Bus/Stop` 回傳物件含 `StopName` 為 `{"Zh_tw": ..., "En": ...}` 物件，不是純字串
- `v3/Bus/StopName` 和 `v2/Bus/StopName` 端點均為 404，已停用

### 公車即時到站（v2）

```bash
GET https://tdx.transportdata.tw/api/basic/v2/Bus/EstimatedTimeOfArrival/City/Taipei
  ?$format=JSON
  &$filter=StopUID eq 'TPE12726'
```

### 新新公園站牌對照表（南港路三段，東西向）

| StopUID | 站牌 | 方向 | 路徑 | 線路 | 備註 |
|---|---|---|---|---|---|
| TPE12726 | 南港站 | 往北基 | 510 | 510、601、602、615 | 西側、往南端 |
| TPE12738 | 南港站 | 往南港 | 510 | 510、601、602、615 | 西側、往北端 |
| TPE12744 | 新新公園 | 往北基 | 601、602、615 | 東側、往南端 | 東側、往南端 |
| TPE12756 | 新新公園 | 往南港 | 601、602、615 | 東側、往北端 | 東側、往北端 |

---

## open-meteo 氣象 + 空氣品質（2026-10-18 整合）

免費、免金鑰，個人使用 rate limit 10,000 req/day。

### 端點（注意兩個不同 host）

```bash
# 天氣預報 + 即時（temperature、humidity、風、WMO 天氣碼、今高低溫）
GET https://api.open-meteo.com/v1/forecast
  ?latitude=25.05&longitude=121.58
  &current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m
  &daily=temperature_2m_max,temperature_2m_min&forecast_days=1
  &timezone=Asia%2FTaipei

# 空氣品質（US AQI）— host 是 air-quality-api，不是 api.open-meteo.com
GET https://air-quality-api.open-meteo.com/v1/air-quality
  ?latitude=25.05&longitude=121.58&current=us_aqi&timezone=Asia%2FTaipei
```

### 座標

- 南港區新新公園：`25.05, 121.58`（寫死在 `src/WxManager.cpp`）

### 顯示（src/main.cpp drawWxStrip）

- 底部 38px 橫條，兩行 6px ASCII 字：
  - 第一行：`<temp>°C <WMO text> H<high>/L<low>`
  - 第二行：`AQI <aqi>  Hum <hum>%  Wind <kmh>`
- 開機 WiFi 成功後立刻抓一次；之後每 10 分鐘重抓
- 公車每 60 秒全清後 `doFetchAndShow()` 會重印氣象條

### 踩過的坑

1. AQI host 錯寫成 `api.open-meteo.com` 會 404，正確是 `air-quality-api.open-meteo.com`
2. open-meteo 回傳 `daily.temperature_2m_max` 是**純 number 陣列** `[32.3]`，不是物件陣列；ArduinoJson 要 `doc["daily"]["temperature_2m_max"][0]` 取今天

### 狀態：運作正常（serial 實測 2026-10-18）

```
[WX] 26C 85% wind 13 km/h [0] H31/L24
[WX] AQI=39
```
