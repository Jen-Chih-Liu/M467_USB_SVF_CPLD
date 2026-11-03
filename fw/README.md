# M467 USB HID／SVF 與背板管理韌體

本文件以目前工作樹的程式與專案設定為準，不以舊註解或歷史映像推定功能。
主程式結合 USB HID、CPLD JTAG/SVF、風扇暫存器鏡像及 NVMe 管理。
M460 是 BSP／專案沿用名稱；目前 Keil Device 為 `M467HJHAE`。
本次僅核對文件，沒有建置、燒錄、操作硬體或執行清理。

## 1. 從哪裡開始

- 主專案：[HSUSBD_HID_Transfer.uvprojx](SampleCode/StdDriver/HSUSBD_svf/Keil/HSUSBD_HID_Transfer.uvprojx)。
- IDE、輸出與燒錄步驟：[Keil/README.md](SampleCode/StdDriver/HSUSBD_svf/Keil/README.md)。
- 主機命令與建置：[app/M460/README.md](../app/M460/README.md)；命令實作仍以原始碼為準。
- ISP 是另一個專案：[ISP_HID_20.uvprojx](SampleCode/ISP/ISP_HID_20/Keil/ISP_HID_20.uvprojx)。
- 一般查詢先選對 USB 裝置；不要以重刷、掃描寫入或 Format 當連線測試。

### 原始碼索引

| 檔案 | 用途 |
| --- | --- |
| [main.c](SampleCode/StdDriver/HSUSBD_svf/main.c) | 時脈、初始化、週期工作與 USB 命令分派 |
| [g_def.h](SampleCode/StdDriver/HSUSBD_svf/g_def.h) | 位址、GPIO 名稱與快取偏移 |
| [cpld_fan.c](SampleCode/StdDriver/HSUSBD_svf/cpld_fan.c) | CPLD、感測器、風扇鏡像與 EEPROM |
| [nvme_mi.c](SampleCode/StdDriver/HSUSBD_svf/nvme_mi.c) | Basic Management 輪詢、mux、MCTP/NVMe-MI |
| [hid_transfer.h](SampleCode/StdDriver/HSUSBD_svf/hid_transfer.h)／[hid_transfer.c](SampleCode/StdDriver/HSUSBD_svf/hid_transfer.c) | HID 端點及收發 |
| [descriptors.c](SampleCode/StdDriver/HSUSBD_svf/descriptors.c) | USB／HID 描述子 |
| [xsvftool-m031.c](SampleCode/StdDriver/HSUSBD_svf/xsvftool-m031.c) | 主專案採用的 JTAG GPIO 實作 |
| [usci_i2c.c](Library/StdDriver/src/usci_i2c.c) | USCI I²C master 輪詢驅動 |

專案沒有加入 [main_mctp_test.c](SampleCode/StdDriver/HSUSBD_svf/main_mctp_test.c)。
它是參考／測試主程式，不能把其中印出的裝置實測敘述當正式韌體保證。
正式 `USCI0_IRQHandler()` 在 [nvme_mi.c](SampleCode/StdDriver/HSUSBD_svf/nvme_mi.c)。
[startup_m460.s](Library/Device/Nuvoton/m460/Source/ARM/startup_m460.s) 的同名 weak 向量由此 strong 定義覆蓋。

## 2. 硬體介面與目前設定

`SYS_Init()` 設定核心 200 MHz，PCLK0／PCLK1 為 HCLK/2。
專案 XML 的 `CLOCK(84000000)` 不是執行時核心時脈。

| 介面 | SDA／RX | SCL／TX | 目前設定／用途 |
| --- | --- | --- | --- |
| I2C0 | PB4 SDA | PB5 SCL | 100 kHz；第一 CPLD、風扇、溫度、EEPROM |
| I2C1 | PB0 SDA | PB1 SCL | 100 kHz；bank0 NVMe mux／SMBus |
| I2C2 | PA10 SDA | PA11 SCL | 100 kHz；第二 CPLD／EEPROM，條件式初始化 |
| UI2C0／USCI0 | PB13 SDA（DAT0） | PB12 SCL（CLK） | **10 kHz 診斷設定**；bank1 NVMe，條件式初始化 |
| I2C4 | PF4 SDA | PF5 SCL | 100 kHz 開啟；向 BMC 提供風扇位址的 slave 鏡像 |
| UART3 | PB14 RX | PB15 TX | 115200、8N1；需外接適當電平的 UART 轉接器 |

UI2C0 是兩線 I²C，不是三線協定；初始化沒有固定啟用其 slave IRQ。
PA10／PA11 的 GPIO 高低檢查影響 `g_u32clpd_lost`。
只有 `g_u32clpd_lost == 0` 才設定 I2C2／UI2C0 腳位並初始化。
這是程式判斷，不等於完整的第二背板功能驗證。

- JTAG：PA7=TCK、PA6=TMS、**PC0=TDI、PC1=TDO**；不要沿用舊文件的反向接法。
- PB3／PB2 是全域琥珀／綠 LED 讀值來源。
- USB GPIO selector `40` 操作 **PC14**；selector `48` 操作 PB6（mux reset 路徑）。
- `HWM_SEL` 巨集雖定義為 PA9，目前管理路由／輪詢判斷用的是 PC14。
- PC14=0 時可進行監控，並啟用 I2C4；PC14=1 時主迴圈關閉 I2C4，PF4／PF5 回到 GPIO input。

### 位址務必區分 7-bit 與 wire 8-bit

| 裝置 | 8-bit 寫入位址 | BSP API 使用的 7-bit 位址 |
| --- | --- | --- |
| CPLD | `0xF0` | `0x78` |
| 溫度感測器 | `0x30` | `0x18` |
| EEPROM | `0xAE` | `0x57` |
| NCT7363Y | `0x40 / 42 / 44 / 46` | `0x20 / 21 / 22 / 23` |
| PCA9848 mux | `0xB2 / B4 / B6` | `0x59 / 5A / 5B` |
| TCA9548 mux | `0xE0` | `0x70` |
| MCTP 目標 SSD | `0x3A` | `0x1D` |
| MCU MCTP slave | `0x20` | `0x10` |

直接 USB `0xD0 / D2 / D4` 的 `[2]` 是 8-bit 位址；程式確實先 `>> 1`。
不要把上述數字再位移一次，也不要把 mux 通道數當裝置實際存在數。

### 風扇與週期工作

啟動時偵測四個 NCT7363Y 位址，再建立各自的暫存器鏡像與備份。
I2C4 只為已偵測到的位址設定 slave address，支援暫存器指標讀取與 byte write。
鏡像寫入不是立刻完成實體風扇 IC 寫入；後續 compare／restore 工作才同步。
[cpld_fan.c](SampleCode/StdDriver/HSUSBD_svf/cpld_fan.c) 的實體暫存器讀取用 PB4／PB5 GPIO bit-bang，寫入用硬體 I2C0。
bit-bang 半週期延遲設定 5 µs，不應宣稱實際波形精確為 100 kHz。
舊 16-fan 管理介面多處在 `#if 0`，主迴圈 `fan_read()` 亦未呼叫。
因此不能把報告 `0x70` 宣稱為持續更新的完整 16／32 風扇資料表。
Timer0 每秒 1000 tick；工作條件是 `timer0_count > 500`，不是硬即時 500 ms 保證。
監控須同時滿足 `i2c_monitor_flag == 1` 與 PC14=0。
I2C4 idle 計數達 30000 且 PC14=0 時，先把鏡像 `0x90..0xAE` 設為 `0xFF`。
此 watchdog 路徑的直接實體寫入呼叫被註解，仍依賴後續同步；不要假定當下已全速。

## 3. USB HID 命令

VID/PID=`0x0416 / 0x502A`；一個 vendor-defined HID interface。
兩個資料端點：`0x81` Interrupt IN、`0x02` Interrupt OUT；EP0 Control=64 bytes。
High-Speed 資料端點最大封包 1024 bytes，Full-Speed 為 64 bytes。
沒有 Bulk 端點或 EP3；FS 描述子存在不代表目前 1024-byte 主機流程已完成 FS 實測。
以下 `[n]` 為 USB buffer 的零起算 byte index，應用命令多以 1024-byte 報告處理。
未回應的操作不可無條件等待 IN；回應也不全是「命令 byte + 狀態」。

| 命令 | 請求／行為 | 回應 |
| --- | --- | --- |
| `0xA0` | 重設 SVF 累積與錯誤狀態 | 清除狀態後送報告；不要假定所有尾端 byte 均清零 |
| `0xA2` | `[1..1023]` 累積長 SVF 命令 | 無 IN 回應 |
| `0xA1` | 加入最後／單包內容並執行 SVF | 前 4 bytes 為錯誤行號 LE／零；內部累積上限 8192 bytes |
| `0xB0` | 韌體版本 | 前 4 bytes：`26 10 20 01`，不是主機 app 版本 |
| `0xB1` | `[1]=5A [2]=A5` 跳 LDROM | reset，無一般回應 |
| `0xB2` | `[1]=55 [2]=AA` 回 APROM；`[1]=5A` 設 reset 變數 | reset／set 無一般回應；`[1]=6A` 查詢回 `[0]=B2 [1]=值` |
| `0xB3` | 讀 LED GPIO | `[0]=B3 [1]=PB3 [2]=PB2` 的當下值 |
| `0xC0 / C4` | 讀 bank0／bank1 報告 | 原始 1024-byte 快取，不是新一輪匯流排讀取 |
| `0xC1 / C5` | `[1..256]` 寫 EEPROM0／1，先切 CPLD `F3=1` 再還原 `F3=0` | 無 IN 回應；C1 用 I2C0，C5 用 I2C2 且須第二 CPLD 可用 |
| `0xC2 / C6` | 讀 EEPROM0／1 的 256 bytes | `[0]=C2/C6 [1..256]=資料`；第二 CPLD 不可用時 C6 的 `[0]=0` |
| `0xD0` | `[1]=bus [2]=addr8 [3]=len [4..]=資料` | 寫入本身無 IN；D1 查詢 byte count |
| `0xD1` | 查上次寫入計數 | `[0]=D1 [1]=BytesWritten` |
| `0xD2` | `[1]=bus [2]=addr8 [3]=len` | 讀取本身無 IN；D3 取結果 |
| `0xD3` | 查上次讀取 | `[0]=D3 [1]=BytesRead [2..]=資料` |
| `0xD4` | `[1]=bus [2]=addr8 [3]=WLen [4]=RLen [5..]=寫資料` | `[0]=D4 [1]=WCount [2]=RCount [3..]=讀資料` |
| `0xDA / DB` | DA 用 `[1]` 設監控；DB 查詢 | DA 無 IN；DB 回 `[0]=DB [1]=flag` |
| `0xDC / DD` | selector 40=PC14、48=PB6；DC 用 `[2]` 設值 | DC 無 IN；DD 回 `[0]=DD [1]=GPIO值` |
| `0xE0` | 單槽 NVMe-MI；詳見下一節 | status、資料或 ASCII 原因 |

**D 系列 bus selector 只有 `0=I2C0`、`1=I2C2`；不是 I2C1，也沒有 selector 2 分支。**
`0xD4` 呼叫 write 再 read，不應稱為已保證 repeated-START 的原子交易。
其他 selector 未做有效交易，可能回報舊計數；主機應先檢查參數。

## 4. NVMe-MI 與快取

請求：`[0]=E0 [1]=global slot（0-based） [2]=CommandID`。
`[3..6]=Dword0 LE`，`[7..10]=Dword1 LE`。
回應：`[0]=E0 [1]=ID [2]=status（0成功／1失敗） [3]=資料長度`。
成功資料從 `[4]` 開始；失敗為 `[4..]` 的 NUL 結尾 ASCII 原因。
一般 live 分支最多回傳 200 bytes，不能把整份 Identify 當單包回應。
主機 [M460.c](../app/M460/M460.c) 使用 1-based `NVMEIndex`，送線前減一。

| ID | 目前分支 | 備註 |
| --- | --- | --- |
| 1 | Basic Management | 只讀 32-byte 快取，不走 MCTP ISR |
| 2 / 3 | Endpoint UUID／Message Type Support | live MCTP Control，無 MIC |
| 4 | Read MI Data Structure | live native NVMe-MI |
| 5 / 6 | Configuration Set／Get | 5 會改裝置設定 |
| 7 / 8 | VPD Read／Write | 8 會改 VPD；7 未指定 DLEN 時預設 `0x20` |
| 9 | Reset | **不支援**，落入 not-implemented 回應 |
| 10 / 11 | Subsystem／Controller Health Poll | 11 的 Dword0=0 時使用 `0x87010000` |
| 12 / 15 / 16 | Health Info+Alerts／Alerts／Info | 共用 native health poll opcode |
| 13 | Power Management | Admin Get Features，回 CQE DWORD0 的 4 bytes |
| 14 / 17 / 18 | FW Revision／SMART／Identify | Admin passthrough；Dword0 低 16 bits 為回傳資料分頁偏移 |
| 19 | Format NVM | 破壞性操作，見安全說明 |

主程式 E0 上方「只實作 ID1」註解已過時，須看實際 `else if` 分支。
目前不能寫成「2..19 全部 live 支援」；ID9 明確未實作。

### VPD Write 的實際格式

主機 [usb_cmd.c](../app/M460/usb_cmd.c) 與韌體一致：`[11]=資料 byte count`、`[12..]=payload`。
長度不在最後一個 USB byte，payload 也不是從 `[7]` 開始。
韌體把 count 限制為最多 180 bytes，取 Dword0 低 16 bits 作 DOFST。
送 NVMe-MI 時 **DLEN 使用此實際 carried count**，而不是原 Dword1 的值。
寫入前應備份 VPD；USB 傳輸成功仍須檢查 NVMe-MI status。

### 兩個 bank 的 flat slot 編號

- bank0 的報告為 `bmc_report`，NVMe bus 是 I2C1。
- bank1 為 `bmc_report1`，NVMe bus 是 UI2C0；存在條件為 `g_u32clpd_lost == 0`。
- global wire index 小於 bank0 count 時取 bank0；否則 bank1 local=`global-bank0Count`。
- 例如 14+14 槽，CLI 槽 28 → wire 27 → bank1 local 13，快取起點 `0x2A0`。
- 每個快取 1024 bytes；從 `0x100` 起每槽 32 bytes，最後一槽 `0x3E0..0x3FF`。
- ID1 驗證 bank0 count 為 `0..24`；取 bank1 時也驗證其 count、存在與索引。
- 無效 bank0 count 不會被當零而誤導到 bank1；不存在／超界回 Fail。
- 24 是**每個快取 bank 的容量**，不是全域 index 必須小於 24。

### live mux 的已知限制

`SelectNvmeSlotChannel()` 沒有 ID1 同等的兩個 bank count／總槽數驗證。
它甚至把 bank0 count=`0xFF` 當零；不可用快取分支的安全性替 live 分支背書。
local `/8` 選 mux，`%8` 選通道；PCA9848 陣列有 3 個位址，理論為每 bank 24 通道。
TCA9548 陣列只有 1 個位址，實際設定為每 bank 8 通道。
live 路由卻用 PCA 陣列長度 3 檢查 TCA index；TCA local≥8 有越界風險。
因此不能宣稱所有 48 槽的 live MCTP 都已驗證；先核對裝板 mux 與實際槽數。

### MCTP 收發生命週期與錯誤

master polling 期間關閉所選 bus 的 slave IRQ；prepare 清理共享 RX 狀態。
TX 必須完整送出、無 driver error 且 STOP 已完成，才 listen／啟用 slave `0x10` 的 RX IRQ。
結束後停用 IRQ、卸下 callback；並非開機後永遠開著 USCI0 接收。
TX 失敗不再送剩餘分片，也不直接重試或重新啟用接收。
輪詢失去 arbitration 不送 master STOP；pending STO 不得被取消以強行重啟。
listen 保留新的 response flags／partial state，不可泛稱應先全部清旗標。
USCI ISR 先處理 fault，再取 RXDAT；合法 `ACK|STOP` 先 append 最後 byte 再 publish。
W1C 依該次 snapshot 清除後才 PTRG；無事件／無 callback 不干涉 master polling。
RX ring 深度 16、可用 15 槽、每槽 128 bytes；不可覆寫 unread packet。
TX payload MTU=64 bytes；逐包驗 PEC（CRC-8），重組訊息依 IC bit 驗 MIC（CRC-32C）。
MCTP Control 無 MIC，不應把所有訊息都寫成 PEC+MIC 必驗。
sticky `-10`=slave fault、`-11`=queue／packet overflow、`-12`=TX 失敗。
`-2` 是 RX 軟體忙等預算耗盡；仍是 loop counter，不能換算成精確秒數。
framing／PEC／MIC 等錯誤亦會回負值，須分辨 transport、MI status 與 NVMe status。

## 5. 建置、映像與更新安全

主專案 Arm Compiler 6.22／Armclang；現存編譯參數是 C99、`-O0`、Cortex-M4／hard-float。
XML 描述 IROM `0..0x7FFFF`（512 KiB）、IRAM `0x20000000..0x2001FFFF`（128 KiB）。
實際 linker 使用 RO base=0、RW base=`0x20000000`，未選自訂 scatter，也未啟用 target memory layout。
既有 linker 參數未列區域長度；不可把 XML 容量當已強制檢查的連結上限。
`CreateHexFile=0`；建置輸出 AXF，兩個 After Build callback 依序轉 raw BIN、產生 SUM BIN。
詳見 [Keil 操作說明](SampleCode/StdDriver/HSUSBD_svf/Keil/README.md)，不要手動省略 startup／BSP 的零散編譯。

[FILESUM.cpp](../app/FILESUM/FILESUM/FILESUM.cpp) 使用兩個檔案參數：輸入 raw BIN、輸出 SUM BIN。
`APROM_SIZE=256*1024`，先填 `0xFF`，輸出固定 262144 bytes。
尾端 8 bytes：`0x3FFF8..0x3FFFB` 是原始長度 LE32，`0x3FFFC..0x3FFFF` 是 checksum LE32。
checksum 是原始 payload 逐 byte 加總後 `& 0xFFFF`，不是密碼學雜湊。
長度／校驗尾碼不得被 payload 覆蓋；工具缺乏可靠的超長輸入檢查，錯誤也可能回傳 0。
**512 KiB 是 Device APROM；256 KiB 是目前封裝／ISP 契約，兩者不能混用。**
[ISP main.c](SampleCode/ISP/ISP_HID_20/main.c) 同樣設定 `ap_rom_size=256*1024` 並讀取尾端長度／校驗值。
ISP 專案 RO base=`0x0F100000`，與主程式 APROM base=0 分開；不是主專案自動產生的 LDROM。
更新前核對映像種類、長度、尾碼及 bootloader，不能把 AXF、raw BIN、SUM BIN 互換。
主機語法是 `M460.exe <Count> UPDATE M463 <映像路徑>`，版本查詢為 `Version MCU`，不是 `UPDATE MCU`。

### 腳本的實際行為

- [build_keil_projects.bat](build_keil_projects.bat) 從**目前目錄**遞迴搜尋所有 uvprojx／uvproj，包含 ISP。
- 優先找 `C:\Keil_v5\UV4\UV4.exe`，再找 tester 使用者下的指定安裝位置。
- 每個專案執行 Build；log 放在專案旁，名稱為完整專案檔名加 `_build.log`。
- 腳本把 0 印為 SUCCESS、1 印為 WARNING、>1 印為 FAILED；它沒有彙總全域失敗退出碼。
- 因此不能以最後 batch exit code 當全部成功；逐份查看 error／warning 與 post-build 輸出。
- [rename_firmware.py](rename_firmware.py) 從目前目錄搜尋第一個同名來源並複製到目前目錄，不會自動建置。
- 它找的是 `APROM_SUM.bin` 與 `LDROM.bin`，不是主專案目前的 output basename；須先核對來源。
- 輸出命名為 `msi_gpu_cp_<yy_mm_dd>_00_00_07.bin`、`msi_gpu_ld_<yy_mm_dd>_00_00_01.bin`。
- 日期取執行當天；版本為腳本內固定字串，不是讀取 USB 韌體版本，勿冒稱自動發布。

### 清理具有破壞性

[fw_clean_all.bat](fw_clean_all.bat) 從目前目錄遞迴刪除 `obj / lst / Objects / Listings / RTE` **整個目錄**。
其中 BIN、AXF、HEX、map 與 RTE 內容都不保留；先備份，沒有刪除前確認。
[fw_keep_bin.bat](fw_keep_bin.bat) 同樣整刪 `lst / Listings / RTE`。
在 `obj / Objects` 只保留該層副檔名 `.bin` 的檔案；刪除其他檔案及全部子目錄。
子目錄中的 BIN 也會消失；兩支腳本都不會清除搜尋範圍外的檔案。

## 6. 驗證與故障排查

- [test_usci_i2c_polling.py](tests/test_usci_i2c_polling.py)：抽取實際驅動，mock ACK／NACK、fault、STOP、prefix／repeated START 與邊界。
- [test_nvme_slot_cache.py](tests/test_nvme_slot_cache.py)：實際 ID1 分支、0..24 count、無效 count、bank 存在、全部 wire indices 與 14+14 的 CLI28。
- [test_mctp_usci_irq.py](tests/test_mctp_usci_irq.py)：ISR snapshot、ACK|STOP、queue overflow、PEC／MIC、TX failure 與 prepare／listen／end。
- 測試在主機用暫存 mock C/C++ harness；需 Windows MSVC／SDK，缺工具可 skip。
- 測試／syntax check 不等於完整 Keil build，也不證明實際 MCU 的時序、電氣或 slave turnaround。
- ID1 的 mock 將 `sprintf` stub 掉；測到狀態／mapping，不證明錯誤文字邊界安全。
- 要測 bank1 ISR，應在確認 mux、槽號與路由後使用**唯讀 live ID6 Configuration Get**，不是 cache ID1。
- 記錄 UART、TX byte count／driver error、IRQ／PROTSTS 與 SDA／SCL／STOP 波形；不要在 ISR 加大量列印。
- UI2C0 降速註解只是 pull-up／rise-time 假說，不能宣稱已證實晶片或硬體故障。
- 檢查電壓、外部上拉、負載電容與線路；上拉阻值須依板規及量測決定，不提供 47 Ω 上拉建議。

**Format NVM（ID19）會破壞資料。** Dword1 必須為 `0x464D5421`（FMT!）才執行。
Dword0 是 CDW10（LBAF／MSET／PI／PIL／SES），NSID 固定 `0xFFFFFFFF`。
SES=0、1、2 的作用不同，不能把未指定 SES 的 Format 宣稱必然完整清除資料。
RX timeout `-2` 只表示沒有取得回覆，可能仍在處理；不是完成證明，也不要自動重送。
cache ID1／SMART 查詢成功不能證明 Format 完成或資料已清除；需依裝置規格另行驗證。

## 7. 授權與交付

本地 [Library](Library) 含 Nuvoton BSP／CMSIS 來源，保留各來源的 copyright 與授權標頭。
部分 Nuvoton 檔案標示 Apache-2.0，Lib(X)SVF 有其自己的 permission notice。
不能把整個目錄一概宣告同一授權；再散布前逐項核對來源與二進位授權。
交付應附原始碼版本、工具鏈、建置 log、map、實際映像長度／尾碼與板上驗證記錄。
