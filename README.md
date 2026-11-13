# CYD 小畫框 · CYD Photo Frame

ESP32-2432S028R 2.8 吋圖片播放器，支援家用 Wi-Fi、手機瀏覽器管理、自動橫直方向與觸控切圖。

目前版本 **0.2.0**。目標為經典 ESP32、4 MB Flash，包含 ST7789 雙 USB 與 ILI9341 板型。實際驗證範圍見 [驗證紀錄](docs/VALIDATION.md)。

## 功能

- 連家用 **2.4 GHz Wi-Fi**，手機留在同一網路即可上傳、排序、刪圖。
- 初次設定與連線失敗時，可使用畫框的備援熱點。
- 畫框顯示管理 IP，並提供每台裝置獨立的 `.local` 網址。
- 管理帳號 `admin`，密碼與該畫框熱點相同，長按 BOOT 可查看。
- JPG、PNG、WebP 批次上傳，最多 12 張；手機先縮圖，單張原始檔限 25 MiB。
- 預設完整等比例顯示，依照片自動切換橫直；也可固定方向，或在上傳時選擇滿版裁切。
- 自動輪播／手動模式、2～3600 秒間隔、亮度調整，圖片與設定保存在板上。
- 長按螢幕切換自動／手動，短點下一張，不需要觸控座標校正。

## 第一次連家用 Wi-Fi

1. 長按 **BOOT 按鈕 1 秒**，查看畫框熱點名稱與密碼。
2. 手機連上 `CYD-Photo-xxxx`，出現「沒有網際網路」時選擇保持連線。
3. 瀏覽器開啟 **http://192.168.4.1**，登入帳號 `admin`，密碼與畫框熱點相同。
4. 在「家用 Wi-Fi」輸入 2.4 GHz 網路名稱、密碼，按「儲存並連線」。
5. 成功後，把手機切回同一家用 Wi-Fi，開啟頁面或畫框顯示的管理網址。之後可將該網址加入書籤。

網址類似 `http://cyd-photo-xxxx.local`；若裝置不支援 `.local`，使用長按 BOOT 顯示的 IP 網址。

家用 Wi-Fi 設定保存在裝置 NVS，重啟會重新連線。連不上時每 30 秒重試，播放與觸控繼續運作，原備援熱點仍可管理。網頁可「忘記家用 Wi-Fi」再重新設定。管理頁不回傳家用 Wi-Fi 密碼。

服務使用區域網路 HTTP，沒有雲端帳號或網際網路遠端服務。請讓手機與畫框位於可互相存取的同一區域網路；訪客網路的裝置隔離可能阻擋連線。

## 上傳與播放

選擇圖片，確認第一張預覽後按「上傳到畫框」。圖卡提供「顯示」、↑／↓ 排序與「刪除」。播放設定提供輪播開關、秒數、亮度與畫框方向。

| 操作 | 功能 |
|---|---|
| 輕點螢幕並放開 | 下一張 |
| 長按螢幕約 0.8 秒 | 切換自動輪播／手動，模式會保存 |
| 短按 BOOT | 下一張 |
| 長按 BOOT 1 秒 | 顯示／關閉連線資訊 |

完整顯示模式會保留整張照片；照片比例與螢幕不同時有黑邊。滿版裁切模式會切掉部分邊緣。橫圖直接準備為 320×240，直圖為 240×320，避免將舊的低解析縮圖再次放大。

## 管理頁預覽

以下使用本專案自製幾何圖與模擬 API，並非 LCD 或家用網路實機驗收證據：

- [電腦版](docs/manager-desktop-demo.png)
- [手機版](docs/manager-mobile-demo.png)

公開 repo 不包含使用者提供的照片、Flash 備份、Wi-Fi 設定或含私人照片的預編譯映像；已有裝置上的照片可繼續保留。原始幾何測試圖在 `test/fixtures/`。

## 硬體

| PlatformIO 環境 | LCD | 板型 |
|---|---|---|
| `cyd-st7789`（預設） | ST7789，BGR、inversion off | 已確認的雙 USB CYD |
| `cyd-ili9341` | ILI9341 | 傳統 CYD；須自行核對實際 LCD |

LCD HSPI：MOSI 13、MISO 12、SCLK 14、CS 15、DC 2、BL 21。
XPT2046 獨立 SPI：MOSI 32、MISO 39、SCLK 25、CS 33、IRQ 36。

本版使用內建 Flash，不使用 microSD、不需要 PSRAM，也沒有 BLE 傳檔或 OTA 更新。

## 建置與安裝

```sh
python3 -m venv .venv
.venv/bin/python -m pip install platformio Pillow pyserial esptool==4.5.1
.venv/bin/pio run -e cyd-st7789 -e cyd-ili9341
.venv/bin/pio run -e cyd-st7789 -t buildfs
.venv/bin/python tools/package.py
```

`data/` 預設不包含照片，可先安裝後透過網頁上傳；想放入自製範例圖，可在 `buildfs` 前執行：

```sh
.venv/bin/python tools/generate_demo_images.py --output data
```

部分 Apple Silicon 環境的舊版 mklittlefs 無法執行，可設定 `PHOTO_MKLITTLEFS=/absolute/path/to/native/mklittlefs` 指定相容 LittleFS 2.x、block 4096／page 256 的原生工具。

首次完整安裝：

```sh
.venv/bin/python tools/flash.py --port /dev/cu.usbserial-XXXX --variant cyd-st7789 --install
```

工具先檢查映像雜湊、經典 ESP32／4 MB，完整備份後才寫入。首次安裝會取代原有程式與圖片區。備份可能包含舊設定，保存在被 Git 忽略的 `.flash-backups/`。

已安裝本專案且分割表一致時，只更新程式、保留照片與設定：

```sh
.venv/bin/python tools/flash.py --port /dev/cu.usbserial-XXXX --variant cyd-st7789 --update-app
```

此流程讀回比對分割表、備份舊 app，只寫入 `0x10000` 的新程式。可用 `--esptool /path/to/esptool.py` 指定既有 PlatformIO 的工具。連接問題可先試 USB-A → Micro-USB 資料線；按住 BOOT、按一下 RESET 再放開 BOOT 可進入下載模式。

| 映像 | 位址 | 分割區大小 |
|---|---|---|
| bootloader.bin | 0x1000 | — |
| partitions.bin | 0x8000 | — |
| firmware.bin | 0x10000 | 1.5 MiB |
| littlefs.bin | 0x190000 | 2.4375 MiB |

掛載失敗不自動格式化。不能把本套件寫入 ESP32-S3 或未核對的板型。

## 測試

```sh
.venv/bin/python -m unittest discover -s test -p 'test_*.py' -v
c++ -std=c++11 -Iinclude test/test_format.cpp -o /tmp/cyd-format-test
/tmp/cyd-format-test
c++ -std=c++11 -Iinclude test/test_gesture.cpp -o /tmp/cyd-gesture-test
/tmp/cyd-gesture-test
c++ -std=c++11 -Iinclude test/test_network.cpp -o /tmp/cyd-network-test
/tmp/cyd-network-test
```

管理頁整合測試另需 Node.js、Playwright 與 Chrome：

```sh
npm install --no-save playwright
node test/browser.cjs
```

可用 `PLAYWRIGHT_MODULE`、`CHROME_PATH` 指定模組與瀏覽器位置。測試使用自製固定圖與模擬 API；通過模擬測試不代表已完成 ESP32 的實際 Wi-Fi 上傳或長時間驗收。

`tools/prepare_images.py` 可將自己的照片轉成 CYD1 資料；預設完整等比例，`--cover` 才裁切。CYD1 是 16-byte header 加 76,800 個 little-endian RGB565 像素，總長 153,616 bytes，附 CRC32。程式以兩個 320 像素列緩衝渲染，不配置全畫面 framebuffer。

## 授權與來源

原始程式與自製測試圖採 [MIT](LICENSE)，第三方元件見 [NOTICE](NOTICE.md)。

- [CYD 社群硬體與雙 USB 設定](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [Espressif Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
