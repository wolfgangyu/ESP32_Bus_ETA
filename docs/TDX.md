# TDX API 端點與金鑰（交通資料）

> 本檔只收錄 **TDX 交通端點**。天氣與空氣品質整合紀錄見 [API_INTEGRATIONS.md](API_INTEGRATIONS.md)（open-meteo 章節）。

## 帳號與金鑰

- 基礎會員已申請，免費會員每月虛擬點數 3 點
- 虛擬點數說明：[tdx.transportdata.tw/pricing](https://tdx.transportdata.tw/pricing)
- 金鑰存放位置：本機 `.env` 檔（gitignored），已同步到 `include/config_local.h`（gitignored）
- **切勿將金鑰上傳至 GitHub**

## 可用端點（實測 2026-10-10）

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
  &$top=10
Authorization: Bearer <token>
```

- 主要回傳欄位：
  - `RouteName`：英文路名（如 "306Shuttle"）
  - `StopName`：站牌名稱
  - `Direction`：0=去程，1=回程
  - `EstimateTime`：**秒數**（0=未啟航/暫無資料；>0 時 ÷60 = 預計幾分鐘後到站）
  - `StopStatus`：0=未知, 1=未啟航, 2=行駛中, 3=即將到站, 4=已到站, 5=已取消
  - `SrcUpdateTime` / `UpdateTime`：ISO8601 時間戳記
- **URL 中 `$` 需編碼為 `%24`**，否則 nginx 回傳 400 Bad Request

### 公車即時到站（v3）

- 端點：`/api/basic/v3/Bus/EstimatedTimeOfArrival/City/Tainan`
- 路徑城市參數不直觀：`Taipei` 回傳 400，需寫 `Tainan`，且**只回台南市資料**（TNN 開頭），台北市查不到
- 不作為台北市專案的資料來源

### NearBy ETA（未實作）

- 端點：`/api/advanced/v2/Bus/EstimatedTimeOfArrival/NearBy`
- 必需參數 `$spatialFilter`，後端拋 `System.NotImplementedException`
- 目前所有 spatial filter 語法均無法使用，待 TDX 上線

### 路線資訊

```bash
GET https://tdx.transportdata.tw/api/basic/v2/Bus/Route/City/Taipei
  ?$format=JSON
  &$filter=RouteID eq '15314'
Authorization: Bearer <token>
```

- 可查路線起訖站、營運時間、第一/最後班車時間等

## 目標站牌

- 全名：**西新里(新新公園)**
- 地址：南港路三段 104 號同向（向西）
- `StopUID`：**TPE12726**
- 同站另一方向（向東，南港路 53 號）：`TPE12858`
- 203 路線停靠此站（往天母方向）
- 其他停靠路線：28、205、306、306區、276、711、668、605、605新台五、605副
  （各路線分佈於 22 支同名牌站牌上，見下表）

### 站牌 → 路線對照（實測 2026-10-17）

TDX 把「西新里(新新公園)」的 22 支站牌各自獨立建模，每支只回報自己那條路線的 ETA。
**查 `StopUID eq 'TPE12726'` 永遠只回 203**，要抓到 205/306/276/605 等路線，
需用 `StopName/Zh_tw contains '新新公園'` 一次抓回全部 22 支。

22 支 StopUID 與對應路線：

| StopUID | 路線 | 方向(0=西,1=東) | 說明 |
|---|---|---|---|
| TPE12726 | 203 | 0 | 南港路三段104號向西 |
| TPE12858 | 203 | 1 | 南港路53號向東 |
| TPE27217 | 28 | 0 | 南港路三段299巷1號向西 |
| TPE27237 | 28 | 1 | 南港路二段188巷1號向東 |
| TPE34703 | 205 | 0 | 南港路三段108號向西 |
| TPE34773 | 205 | 1 | 南港路53號向東 |
| TPE36886 | 306 | 0 | 南港路三段108號向西 |
| TPE36925 | 306 | 1 | 南港路53號向東 |
| TPE44134 | 276 | 0 | 南港路三段108號向西 |
| TPE44180 | 276 | 1 | 南港路53號向東 |
| TPE44368 | 306區 | 0 | 南港路三段108號向西 |
| TPE44413 | 306區 | 1 | 南港路53號向東 |
| TPE45108 | 711 | 0 | 南港路三段108號向西 |
| TPE45143 | 711 | 1 | 南港路53號向東 |
| TPE45217 | 668 | 0 | 南港路三段108號向西 |
| TPE45270 | 668 | 1 | 南港路53號向東 |
| TPE45995 | 605 | 0 | 南港路三段108號向西 |
| TPE46046 | 605 | 1 | 南港路53號向東 |
| TPE46094 | 605新台五 | 0 | 南港路三段108號向西 |
| TPE46145 | 605新台五 | 1 | 南港路53號向東 |
| TPE46235 | 605副 | 0 | 南港路三段108號向西 |
| TPE46286 | 605副 | 1 | 南港路53號向東 |

### 抓法

```bash
# 一次抓回全部 22 支站牌（目前韌體用法）
GET /api/basic/v2/Bus/EstimatedTimeOfArrival/City/Taipei
  ?$format=JSON
  &$filter=contains(StopName/Zh_tw,'新新公園')
  &$top=100
```

`EstimateTime=0` 代表暫無資料（該路線此刻沒有車次），`null` 同義。
TDX rate limit 很嚴，`$top=100` 一次拿全量，避免多次查詢觸發 429。

### 端點可用性速查（2026-10-17 實測）

| 端點 | 狀態 | 備註 |
|---|---|---|
| v2/Bus/Stop/City/Taipei | ✅ | 站牌清單 |
| v2/Bus/Route/City/Taipei | ✅ | `_expand=RouteStop` 回傳 0 筆（TDX 未填充），`SubRoutes` 可用 |
| v2/Bus/RouteStop/City/Taipei | ❌ 404 | 端點不存在（v1 也是 404） |
| v2/Bus/SubRoute/City/Taipei | ❌ 404 | 端點不存在 |
| v2/Bus/EstimatedTimeOfArrival/City/Taipei | ✅ | 主要端點，用 StopName 查 |

> v3 端點（Route / SubRoute / EstimatedTimeOfArrival）路徑城市參數不直觀：
> `Taipei` 回 400，只接受 `Tainan`，且**只回台南市資料**（TNN 開頭），
> 台北市完全查不到，不作為本專案資料來源，後續不用試。

### EstimateTime 語義與顯示策略

- `EstimateTime` 為 **JSON null**（TDX 常態）或 `0`：代表 TDX 當下無 GPS 回報該路線，
  **不是「0 秒後到」**。韌體端 `BusData::parseFromAPI` 一律跳過（`estSecs <= 0` continue），
  避免把「暫無資料」顯示為 0 分鐘誤導乘客。
- 顯示語法：`<60s` → "Now"；其餘 → `~N min`（四捨五入），全 ASCII。
- `StopStatus` 僅輔助：3="Arriving"、4="At stop"，其餘留空（不顯示）。

### 路線英文名對照（螢幕用）

TDX `RouteName.En` 直接用，全 ASCII，不需自行轉換：

| Zh_tw | En（TDX 回傳） |
|---|---|
| 306 | 306 |
| 306區 | 306Shuttle |
| 605 | 605 |
| 605副 | 605Sub |
| 605新台五 | 605Xintaiwu |
| 605快 | 605Express |
| 其餘數字路線 | 同數字（如 "203"、"205"、"276"、"668"、"711"、"28"） |

## API 範例程式碼

- 官方範例：[github.com/tdxmotc/SampleCode](https://github.com/tdxmotc/SampleCode)（C# / Python3 / PHP / JAVA / JS / R / GoogleAppsScript）
- **Arduino/ESP32 範例：無**，需自行撰寫（本專案 TDXClient 即為此用途）

## Swagger 文件

- Bus（舊版）：[tdx.transportdata.tw](https://tdx.transportdata.tw/api-service/swagger/basic/30bc573f-0d73-47f2-ac3c-37c798b86d37)
- CityBus（現行）：[tdx.transportdata.tw](https://tdx.transportdata.tw/api-service/swagger/basic/2998e851-81d0-40f5-b26d-77e2f5ac4118)
