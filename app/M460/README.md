# M460 USB 管理工具

本工具透過 libusb 管理 Nuvoton M460/M463 背板韌體，提供 CPLD、
磁碟快取資訊、NVMe-MI、風扇、溫度、GPIO、FRU 與韌體更新操作。
本文依目前程式的參數解析、封包組裝及韌體執行分支整理，不以註解當成功能保證。

- 主機版本字串：`20260420`，來自 [config.h](config.h)，不是硬體韌體版本或穩定版認證。
- 正常模式 VID/PID：`0x0416 / 0x502a`；LDROM ISP 模式：`0x0416 / 0x3f00`。
- USB 裝置表最多保留 16 台；每次掃描依 Bus、Port Path 排序。
- 本文未執行建置、跨平台測試或硬體驗證；實際支援仍取決於板型、韌體及磁碟。
- 寫入暫存器、調整 GPIO、更新韌體、改寫 VPD/FRU、格式化均有副作用。

## 1. 原始碼與用途

| 檔案 | 實際用途 |
| --- | --- |
| [M460.c](M460.c) | `cmd_table`、命令解析及各命令輸出 |
| [config.h](config.h) | VID/PID、版本、封包大小、報表映射與資料結構 |
| [usb_cmd.c](usb_cmd.c) | USB 掃描、介面取得、interrupt 傳輸與封包封裝 |
| [mcu_isp.c](mcu_isp.c) | LDROM 搜尋、ISP 握手、APROM 更新 |
| [cpld_svf.c](cpld_svf.c) | SVF 文字處理與 USB JTAG 傳送 |
| [cJSON.c](cJSON.c)、[cJSON.h](cJSON.h) | JSON 產生 |
| [CMakeLists.txt](CMakeLists.txt) | `M460` 執行檔及 libusb 連結設定 |
| [CMakePresets.json](CMakePresets.json) | IDE／CMake configure presets |

對應韌體分派見 [main.c](../../fw/SampleCode/StdDriver/HSUSBD_svf/main.c)，
NVMe-MI 傳輸與 mux 路由見 [nvme_mi.c](../../fw/SampleCode/StdDriver/HSUSBD_svf/nvme_mi.c)。
韌體建置說明另見 [fw/README.md](../../fw/README.md)。

## 2. 建置設定與執行環境

### Windows：目前設定採 x64

1. 準備 MSVC C/C++ 工具鏈、Windows SDK 與 CMake；Ninja presets 另需 Ninja。
2. 在 VS Code CMake Tools 將本目錄選為來源目錄，選取 x64 工具鏈／開發環境。
3. 選 configure preset `x64-debug` 或 `x64-release`，先 Configure，再 Build `M460`。
4. 也可選 Visual Studio 多組態產生器，再選 Debug／Release 組態建置。
5. 執行時保留建置後複製至執行檔同目錄的 libusb DLL。

[CMakeLists.txt](CMakeLists.txt) 宣告最低 CMake 3.8，但使用了較新的功能；
不應據此保證 3.8 可完成建置。MSVC 靜態 runtime 選擇依賴 `CMP0091`，
一般設定建議 CMake ≥ 3.15；使用本版本 3 的 preset schema 則需 CMake ≥ 3.21。
MSVC runtime 設為 Release `/MT`、Debug `/MTd`，不代表 libusb 已靜態連結。

Windows 連結路徑固定為 `LIB_PATH_SUFFIX = VS2019/MS64`，使用該目錄的
DLL import library，並以 `$<TARGET_FILE_DIR:M460>` 作為 DLL 複製目的地。
此資料夾是預編譯函式庫的分類，不是強制使用 VS2019 編譯器；
使用 VS2022 不需要因此重新命名資料夾。工具鏈、SDK 與函式庫相容性仍需確認。

### 實際 presets 與產物位置

| configure preset | 產生器／組態 | 注意事項 |
| --- | --- | --- |
| `windows-base` | Ninja；隱藏基底 | 指定 `cl.exe`，僅 Windows |
| `x64-debug` | Ninja／Debug | x64 架構由外部環境提供 |
| `x64-release` | Ninja／Release | 繼承 x64 Debug，覆寫組態 |
| `x86-debug`、`x86-release` | Ninja／Debug、Release | 雖存在，固定 MS64 連結設定不適用 x86 |
| `linux-debug` | Ninja／Debug | 僅 Linux，未保證可編譯 |
| `macos-debug` | Ninja／Debug | 僅 Darwin，未保證可編譯 |

目前沒有 `buildPresets` 或 `testPresets`；configure preset 名稱不能當作
build preset 使用。`architecture.strategy = external` 不會自行建立 MSVC 開發環境。
Ninja presets 的 binaryDir 為來源目錄下的 `out/build/<presetName>`；
單組態產物通常直接位於該目錄。Visual Studio 多組態產物則通常在
所選 binaryDir 的 Debug／Release 子目錄，不一定是固定的 build/Release。
preset 的 installDir 只是設定值，目前沒有安裝規則。

### Linux／macOS 分支與 USB 驅動

非 WIN32 建置分支以 pkg-config 搜尋 `libusb-1.0`；來源也有 POSIX 比較、延遲分支。
但 [cpld_svf.c](cpld_svf.c) 直接使用 Windows 反斜線 include 路徑，
[M460.c](M460.c)、[mcu_isp.c](mcu_isp.c) 尚有 `_stricmp` 等 Windows 相依用法。
因此這些分支不是 Linux／macOS 已建置或已測試的承諾，本文不提供保證可用的流程。

Windows 的目標 USB 介面需有 libusb 可相容存取的驅動，例如 WinUSB。
裝置宣告 HID 並不代表系統預設 HID 驅動就能讓本工具取得介面。
請先核對目標介面及既有軟體需求；不要任意替換其他裝置的驅動，
也不要把安裝 libusb-win32 當作本工具的通用修復方式。

### USB 介面與報表

[descriptors.c](../../fw/SampleCode/StdDriver/HSUSBD_svf/descriptors.c) 與
[hid_transfer.h](../../fw/SampleCode/StdDriver/HSUSBD_svf/hid_transfer.h) 定義介面 0 為 HID，
IN=0x81、OUT=0x02，端點型態為 interrupt，目前高速設定最大封包 1024 bytes，不是 bulk。
主機從描述符取得端點並呼叫 `libusb_interrupt_transfer()`；一般 OUT 封包為 1024 bytes。
Windows 接收緩衝區配置 1025 bytes、其他平台 1024 bytes，並非不同的 wire 封包大小。
LDROM ISP 另走 64-byte 封包，不能套用正常模式的 1024-byte 管理報表。

## 3. CLI、索引與輸出規則

```text
M460.exe <Count> <Command> [Arguments...]
```

- `Count` 是十進位、0-based USB 邏輯索引；應在目前 `usblist` 結果範圍內。
- 即使 `usblist`、`appver` 不使用硬體索引，CLI 仍需要這個位置參數。
- 命令比對不分大小寫；數值基底依各命令而異，不可全部當十六進位。
- USB 包裝函式多將索引轉為 `unsigned char`；不是所有非法索引都會被拒絕。
- `main()` 丟棄 handler 回傳值：命令找到後即回傳 0，**退出碼 0 不等於成功**。
- 缺少頂層參數或未知頂層命令回傳 1；各 handler 錯誤不會可靠反映至程序狀態。
- 部分分支的參數數量檢查不足；務必提供完整參數，不要靠錯誤提示測試邊界。

`dbg_printf`、`cmd_printf`、`json_printf` 都映射為 `printf`。
stdout 可能先出現 `[CMD]`、`updated usb table, ...` 等診斷文字，再接 JSON；
部分雙背板命令輸出兩個 JSON 物件，不能對整份 stdout 直接 `json.loads()`。
`dumpall` 與 FRU READ 是文字輸出，各命令也沒有共同的 JSON 基底／錯誤碼表。
自動化應保存 stdout、stderr，按命令解析結果，不可只檢查 `$LASTEXITCODE`。

### 頂層命令一覽

| 命令 | 可用操作／限制 |
| --- | --- |
| `CPLD` | `Id`、`Version`、`Get`、`Set` |
| `HDD` | `Info`、`Port Status`、`LED Status`、`Temp`；`NVMe-Mi` 僅提示 |
| `NVMEMI` | 單槽 CommandID 操作，詳見第 5 節 |
| `Update` | `M463` 或 `CPLD` 加輸入檔 |
| `Version` | `MCU` |
| `BPB` | `Sensor Get` |
| `Reset` | 單獨重置、`Setval`、`Getval` |
| `I2CBYPASS`、`I2CBYPASS1` | I2C0／I2C2 的 `Get`、`Set` |
| `FAN` | `RPM Get`、`Duty Get`、`Duty Set` |
| `Global` | `LED Status` |
| `usblist`、`appver` | 裝置表／主機版本；`-v` 只增加提示文字 |
| `gpio` | `Setval`、`Getval` |
| `dumpall` | 板卡與磁碟快取診斷文字 |
| `FRU` | `READ0`、`READ1`、`WRITE0`、`WRITE1` |

## 4. 查詢與背板監控

以下唯讀查詢範例假設執行檔位於目前目錄，USB 索引 0 為目標裝置：

```powershell
.\M460.exe 0 usblist
.\M460.exe 0 appver
.\M460.exe 0 Version MCU
.\M460.exe 0 CPLD Id
.\M460.exe 0 CPLD Version
.\M460.exe 0 CPLD Get 0x30 1
.\M460.exe 0 HDD Info
.\M460.exe 0 HDD Port Status
.\M460.exe 0 HDD LED Status
.\M460.exe 0 HDD Temp
.\M460.exe 0 BPB Sensor Get
.\M460.exe 0 FAN RPM Get
.\M460.exe 0 FAN Duty Get
.\M460.exe 0 Global LED Status
.\M460.exe 0 Reset Getval
.\M460.exe 0 GPIO Getval 40
.\M460.exe 0 dumpall
```

- `usblist` 產生 `USBCount`、`USB0` 等 Bus／Port 字串，沒有序號欄位。
- `Version MCU` 的 `MCU_Version` 是讀回 32 位元值右移 8 位後的十六進位字串。
- `CPLD Id/Version` 可附第二顆的 `CPLD1_ID/CPLD1_Version`；以第二報表版本非 0／FF 判斷。
- `CPLD Get <reg> <len>`：暫存器以十六進位解析；長度十進位，已檢查 1..32。
- `CPLD Set <reg> <value>`：兩者十六進位，經一位元組介面傳送；屬硬體寫入。
- `HDD Info` 解碼每槽 32-byte 快取：`product` 是 VID 廠商映射、`SN` 是序號，`Version` 固定 `NA`。
- 它沒有輸出 Model、Capacity 或 Present 布林欄位；型號／韌體識別可參考 Cmd18 原始資料。
- `HDD Port Status` 回傳字串 `SATA Present`、`NVME Present` 或 `false`。
- `HDD LED Status` 從存在狀態及 0x50 區域解碼 `Standby/Fault/Locate/Rebuild`，空槽為 `NA`。
- **目前 handler 只接受 HDD LED Status，沒有 HDD LED Get／Set，也沒有 NVME<n> 設定語法。**
- `HDD NVMe-Mi ...` 只印訊息，不執行磁碟命令；真正入口是頂層 `NVMEMI`。
- `HDD Temp` 直接把快取 byte 3 轉為 `int8_t` 字串，僅 NVMe Present 槽有數值。
- 因此 0x80／0x81 等特殊編碼可能顯示負數，不應直接視為有效攝氏溫度。
- `BPB Sensor Get` 回傳 `BPB_Temp`，13-bit 有號原始值乘 0.0625，格式化為兩位小數。
- FAN 查詢只有 `FANcount: "1"` 與 `FAN1` 字串，不是四風扇清單。
- RPM 算式為 `1350000 / fan_rpm`，未防止計數值為零；Duty Get 為整數 `raw*100/255`。
- `Global LED Status` 回傳 `GLED_AMB_N_R/GLED_GRN_N_R` 的 `"0"/"1"`，不是通用 LED bitmask。
- `dumpall` 輸出版本、JTAG ID、溫度、三組暫存器及 Basic Management 解碼，不是完整 JSON。

雙背板的 `HDD Info/Temp/LED Status` 各輸出一份本地編號從 NVME1 起的 JSON。
`HDD Port Status` 合併計數，但第二背板 key 固定從 NVME15 起，
不是依第一背板槽數銜接；第一背板超過 14 槽可能產生重複 key。
這些輸出編號不能直接當作 `NVMEMI` 的平坦索引。

## 5. NVMe-MI 單槽命令

```text
M460.exe <Count> NVMEMI <CommandID> <NVMEIndex> [Dword0] [Dword1]
```

`CommandID`、`NVMEIndex` 是十進位；`NVMEIndex` 為 1-based，送出前減 1。
DWORD 以 `strtoul(..., 0)` 解析，建議使用明確的 `0x` 十六進位格式或無前導零十進位。
主機只檢查索引 ≥ 1，沒有完整上限檢查；例如 257 減 1 再截成 byte 會變成 wire index 0。
請自行限制在實際報告的有效槽數，不能把此截斷誤認為安全邊界檢查。

| ID | 名稱／執行路徑 | DWORD／資料語意 |
| --- | --- | --- |
| 1 | Basic Management；報表快取 | 回傳 32 bytes，不發起即時 MCTP 交換 |
| 2 | Get Endpoint UUID；MCTP Control 0x03 | UUID 資料；DWORD 未使用 |
| 3 | Get Msg Type Support；MCTP Control 0x05 | 類型數量及清單；DWORD 未使用 |
| 4 | Read MI Data Struct；native opcode 0x00 | DW0 `[31:24]=DTYP`、`[23:16]=PORTID`、`[15:0]=CTRLID` |
| 5 | Configuration Set；native 0x03，**寫入** | DW0 低 byte 為 Config ID，其他值依設定項目 |
| 6 | Configuration Get；native 0x04 | DW0 `[7:0]=Config ID`、`[31:24]=Port ID` |
| 7 | VPD Read；native 0x05 | DW0 `[15:0]=DOFST`、DW1 `[7:0]=DLEN`；DW1 為 0 時改用 0x20 |
| 8 | VPD Write；native 0x06，**寫入** | 另用 offset 加 byte 清單，見下文 |
| 9 | Reset | 未實作；會失敗，不是第 19 個受支援命令 |
| 10 | Health Status Poll；native 0x01 | DW1 bit31 可清除狀態；唯讀查詢維持 0 |
| 11 | Ctrl Health Poll；native 0x02 | DW0 `[15:0]=SCTLID`、`[23:16]=MAXRENT`、`[31:24]=flags` |
| 12 | Health Info+Alerts；native 0x01 | 與 10 同一路徑及 DWORD 語意 |
| 13 | Power Management；Admin Get Features 0x0A | FID=0x02、SEL=0；回傳 4-byte little-endian CQE DW0 |
| 14 | FW Revision；Admin Get Log Page 0x02 | LID=0x03，64-byte FW Slot Info；DW0 為輸出 byte offset |
| 15／16 | Health Alerts／Health Info；native 0x01 | 與 10／12 共用結構，並非不同 opcode |
| 17 | SMART Log；Admin Get Log Page 0x02 | LID=0x02，512-byte log；DW0 為輸出 byte offset |
| 18 | Device Identify；Admin Identify 0x06 | CNS=1，4096-byte Controller 資料；DW0 為輸出 byte offset |
| 19 | Format NVM；Admin 0x80，**破壞性** | DW0=CDW10；DW1 必須等於確認碼 |

目前韌體有 18 個已實作 ID，磁碟未必支援全部操作。
Cmd4 的 DTYP 0／1／2／3 分別查 Subsystem／Port／Controller List／Controller Info；
DTYP3 應使用 DTYP2 回傳的有效 Controller ID，而不是假定 ID=0。
Cmd5 已實作；設定 SMBus 頻率時 DW0 `[11:8]` 為頻率值、`[31:24]` 為 Port ID，
例如測試腳本採頻率值 1。修改頻率可能中斷通訊，不列為一般唯讀檢查。
Cmd11 的 DW0=0 會由韌體替換為 `0x87010000`。
native 回傳資料保留 3-byte Management Response DWORD，後面才是結構／payload；
不要把它與 Basic Management 的 32-byte 格式或 Admin 資料格式混用。
Cmd14／17／18 每次最多回傳 200 bytes；DW0 先轉成 16-bit 輸出位移，DW1 未使用。
分頁是再次請求後擷取視窗，不保證多頁來自同一個時間點的快照。

### 雙 bank 與已知容量限制

第一 bank 走 I2C1，第二 bank 走 UI2C0；第二顆 CPLD 必須被韌體判定存在。
平坦索引依第一 bank 的實際報告槽數切分：第一 bank 有 N 槽時，N+1 才是第二 bank 首槽。
兩個 1024-byte 快取都從 0x100 放 32-byte 槽資料，每 bank 容量 24，合計最多 48。
Cmd1 會核對快取容量、bank 計數及本地索引，但成功只表示讀到了可存取的快取。

PCA9848 陣列有三個 mux、每個八通道；TCA9548 陣列目前只有一個 mux。
即時 selector 卻以 PCA 的三個 mux 作上限，TCA 分支本地槽 ≥ 8 有陣列越界風險；
selector 也將第一 bank 的 0xFF 計數當作 0，且未完整核對第二 bank 報告槽數。
因此 48 是報表／PCA 容量，不是所有板型可安全執行 48 槽即時命令的保證。
HDD 的部分 handler 也直接依 `hdd_amount` 迭代 24 槽陣列，異常計數可能越界。

### 唯讀範例與回應

```powershell
.\M460.exe 0 NVMEMI 1 1
.\M460.exe 0 NVMEMI 4 1 0x02000000
.\M460.exe 0 NVMEMI 6 1 0x01000001
.\M460.exe 0 NVMEMI 7 1 0x00000000 0x20
.\M460.exe 0 NVMEMI 17 1 0x00000000
.\M460.exe 0 NVMEMI 17 1 0x000000C8
.\M460.exe 0 NVMEMI 18 1 0x00000000
```

JSON 包含 `NVMEIndex/CommandID/Command/Status/Data`。
成功的 `Data.Count` 是實際 byte 數，`Data["Raw data"]` 是逗號分隔大寫十六進位字串；
Cmd1 應為 32 bytes，不是四位元組的假資料範例。失敗通常有 `Data.Error`。

### VPD Write 與 Format NVM 安全事項

Cmd8 語法為 `NVMEMI 8 <NVMEIndex> <DOFST> <byte0> <byte1> ...`，不是一般 DW1。
主機最多保留 180 bytes；每個輸入會截成 byte，超過的資料會被截掉而非報錯。
USB request `[11]` 帶資料數量、`[12..]` 帶資料；韌體用該數量產生 DLEN。
請先備份並確認磁碟 VPD 範圍，本文不提供可直接執行的寫入範例。

**Cmd19 可能造成不可復原的資料損失，NSID 固定為 0xFFFFFFFF（所有 namespaces）。**
DW1 確認碼為 `0x464D5421`（FMT!）；確認碼只是防誤觸，並非備份或安全驗證。
CDW10 `[3:0]=LBAF`、bit4=MSET、`[7:5]=PI`、bit8=PIL、`[10:9]=SES`。
SES 從 bit9 開始，不是 bit8；SES=0 不要求 secure erase，不能據此證明安全清除。
SES=1／2 為 User Data Erase／Cryptographic Erase，仍須確認磁碟能力與適用範圍。

主機把錯誤字串 `NVMe Admin Status 0x01` 或 `NVMe Admin transport error (-2)`
轉成 `Status: InProgress`；這只是啟發式，逾時也可能是通訊失敗，不能證明已開始或完成。
不要因逾時反覆下 Format。**Cmd1 快取 Success 絕不代表格式化完成**；
Cmd17 成功也只證明該次讀取成功，不是獨立的格式化完成／資料清除證據。
程式輸出的 Hint 有過度推論，維運應以磁碟完成狀態及主機端驗證判定。

## 6. I2C、GPIO、風扇、FRU 與更新

| 語法（前面加 M460.exe 與 Count） | 參數／副作用 |
| --- | --- |
| `I2CBYPASS Get <addr> <reg> <len>` | I2C0；addr/reg 十六進位、len 十進位 |
| `I2CBYPASS1 Get <addr> <reg> <len>` | I2C2；相同格式 |
| `I2CBYPASS[1] Set <addr> <reg> <value>` | 實際命令選前述兩者之一；十六進位、單 byte 寫入 |
| `GPIO Setval <pin> <value>`／`GPIO Getval <pin>` | pin/value 十進位；僅 pin40=PC14、pin48=PB6 有韌體分支 |
| `FAN Duty Set <percent>` | 寫 NCT7363 addr 0x46、reg 0x90 |
| `FAN Duty Set <fanid> <percent>` | fanid 只用於提示，仍寫同一暫存器，不支援獨立選扇 |
| `Reset`／`Reset Setval <value>`／`Reset Getval` | 單獨重置；變數值十六進位，截成 byte |
| `FRU READ0`／`FRU READ1` | 讀取 256 bytes，印 16×16 hex dump，不輸出 binary 檔 |
| `FRU WRITE0 <file>`／`FRU WRITE1 <file>` | 輸入必須為副檔名 .bin 且恰好 256 bytes；改寫外部 FRU EEPROM |
| `UPDATE M463 <file>` | .bin 必須恰好 256 KiB；切 LDROM、ISP 寫 APROM、送回 APROM 命令 |
| `UPDATE CPLD <file>` | 輸入 SVF 文字；停止監控、USB JTAG 傳送、嘗試恢復監控 |

I2C bypass 的 addr 是 **8-bit 位址表示法**：主機原樣放入封包，韌體再右移 1 交給 I2C API。
例如 0xF0 對應 7-bit 0x78；不可直接把 CLI addr 解讀成 7-bit。
Get 應自行限制長度 1..32；程式只檢查大於零，沒有檢查 32 上限，這是安全建議而非既有保護。
USB 回覆長度／I2C 完成數的驗證也不完整，Get 成功不能保證所有要求 bytes 都有效。

GPIO 建議 value 只用 0／1；pin40 設 0 會觸發 CPLD 報表重新讀取，與板級選擇相關。
FRU 讀寫會備份 GPIO40、設 0、操作後嘗試還原；GPIO 步驟的失敗未完整驗證。
FRU 不是通用 MCU 內部 EEPROM 命令；沒有頂層 `EEPROM`。
FAN 百分比請限 0..100；程式只夾住大於 100 的值，負值未保護，百分比也不等於實際 RPM。
FAN 的含 `.json` 參數分支只印載入提示，沒有開檔或解析 schema，結果回報 False。

MCU ISP 以 VID/PID 重新尋找第一台 LDROM 裝置，沒有保留原 `Count` 的拓樸綁定；
更新時避免多台同時處於 LDROM。它檢查封包 ACK／序號並印 checksum，
但更新路徑沒有比較最終 checksum，因此不宣稱完整映像 hash／讀回驗證。
SVF 完成狀態也受 USB 回覆與向量內容影響，不是所有結果都有獨立驗證。
沒有 `UPDATE MCU`、`MCU Version`、`MCU Reset` 或獨立 `MCU LDROM` 命令。

## 7. 測試腳本與故障排查

[app/test_nvmemi.py](../test_nvmemi.py) 預設磁碟案例是唯讀 ID：
1、2、3、4、6、7、10、11、12、13、14、15、16、17、18；不包含 5／8／19／9。
**但每次會先執行 GPIO Setval 40 0，且不備份還原，所以整個腳本不是零副作用。**
只有 `--only` 明確列出 5 才會變更設定；8／19 另需 `--danger`，仍有資料損失風險。
腳本把非 Success（含 InProgress）視為 FAIL，SKIP 也會使總結非零。

執行檔依序搜尋 build/Debug、build/Release、腳本所在目錄，最後 PATH；
`--bin` 可覆寫路徑，存在檔案會轉成絕對路徑，找不到則在硬體設定前停止。
不會自動搜尋 Ninja preset 產物，需自行指定。以下從儲存庫根目錄執行，
索引 28 必須確實存在；它屬哪個 bank 取決於第一 bank 報告槽數：

```powershell
python app/test_nvmemi.py --usb 0 --index 28 --only 1,6
```

[app/test.py](../test.py) 會寫 CPLD、Reset 變數與 I2C 暫存器，不是唯讀巡檢；
其直接解析整份 stdout 的方法也可能遇到診斷前綴而失敗，勿無審核批次執行。

- 找不到裝置：確認供電、線材、正常／LDROM VID/PID，再重新核對 `usblist`；索引不是永久身分。
- 開啟／claim 失敗：確認目標介面驅動、權限與其他程式占用，管理員權限不會自動修正驅動不相容。
- DLL 錯誤：確認 x64 執行檔旁的 libusb DLL 與連結設定相符；靜態 MSVC runtime 不會消除 DLL 需求。
- NVMe-MI 逾時：核對 PC14、有效槽數、bank、mux 類型、磁碟 MCTP 能力及完整 Error 字串。
- 一般查詢多使用 1000 ms USB IN timeout；NVMEMI 使用 60000 ms；部分 OUT timeout=0 可無限等待。
- timeout 在各傳輸呼叫中，不是統一可在設定檔修改的值；不要把逾時一律當磁碟忙碌。
- 更新失敗：確認映像大小、板型、輸出原因及裝置重新列舉；不要靠退出碼或盲目重置判斷。
- 本文不承諾跨平台可用、全槽即時可用或格式化完成；相關未知需另行建置及硬體驗證。
