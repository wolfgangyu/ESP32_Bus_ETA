# Task 1 Brief: 觸控功能啟用 (XPT2046)

**BASE:** 19c673c

## Steps

### Step 1.1: 修改 platformio.ini build_flags 加入觸控定義
**File:** `platformio.ini:26-44`
**Action:** 在現有 build_flags 區塊追加觸控腳位定義

### Step 1.2: 修改 include/config.h 加入觸控校準預設值
**File:** `include/config.h:43` 之後
**Action:** 新增 TOUCH_CAL_X0/Y0/X1/Y1

### Step 1.3: 修改 src/TouchManager.cpp:15-21 完善 begin()
**File:** `src/TouchManager.cpp`
**Action:** 呼叫 tft->initTouch()、套用預設校準值

### Step 1.4: 修改 platformio.ini build_src_filter 加入 TouchManager
**File:** `platformio.ini:45-51`
**Action:** build_src_filter 加入 +<TouchManager.cpp>

### Step 1.5: 修改 src/main.cpp setup() 初始化觸控
**File:** `src/main.cpp:250` 之後 (tft.init() 之後)
**Action:** touch.begin(&tft);

### Step 1.6: 建置驗證
**Command:** `~/.platformio/penv/bin/pio run -e esp32dev`
**Expected:** 編譯通過、無警告

### Step 1.7: 燒錄實測觸控座標
**Command:** 燒錄 + pio device monitor
**Expected:** 觸摸螢幕四角與中心，序列埠輸出座標對應視覺位置

### Step 1.8: Commit
**Command:** git add ... && git commit -m "feat(touch): enable XPT2046 touch with default calibration"
