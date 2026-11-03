# HSUSBD_HID_Transfer：Keil 建置與燒錄

本文件只說明目前主專案；完整 USB／NVMe 協定與限制見 [fw/README.md](../../../../README.md)。
設定依 [HSUSBD_HID_Transfer.uvprojx](HSUSBD_HID_Transfer.uvprojx)、原始碼及現存參數檔核對。
歷史輸出存在不等於最新原始碼已建置成功；本次沒有執行 build、download 或 cleanup。

## 1. 開啟正確專案

1. 在 Windows 的 Keil µVision 使用 **Open Project** 開啟上述主專案。
2. 選擇 target `HSUSBD_HID_Transfer`，進入 **Options for Target**。
3. 核對 Device=`M467HJHAE`、Arm Compiler=`6.22`，不要切回 ARMCC 5。
4. 安裝／核對可支援該 Device 的 Nuvoton Device Pack；BSP 程式碼取自本地 [Library](../../../../Library)。
5. 核對本目錄 [FILESUM.exe](FILESUM.exe) 與 compiler 的 `fromelf` 可用。
6. 初次或更換工具鏈時使用 **Rebuild**；一般增量更新使用 **Build**。
7. 查看 **Build Output** 的 error、warning、link 與兩個 After Build 結果，不以進度列判定成功。

專案相對 include／source 路徑依此目錄解析，不要把單一來源搬到其他目錄直接編譯。
不提供容易混淆版本的快捷鍵，也不以手工列少量 object 取代正式專案。

## 2. 目前 compiler 與 linker

| 項目 | 核對結果 |
| --- | --- |
| Device／CPU | `M467HJHAE`；Cortex-M4、single-precision FPU |
| Compiler | `pCCUsed=6220000::V6.22::ARMCLANG`、`uAC6=1` |
| C 設定 | XML `Optim=1`；現存參數為 `-std=c99 -O0`，不是 `-O2` |
| CPU flags | `--target=arm-arm-none-eabi -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard` |
| 其他 flags | `-ffunction-sections`、short enum／wchar、`__MICROLIB`；LTO 未啟用 |
| Output | `OutputDirectory=.\obj\`、`OutputName=HSUSBD_HID_Transfer` |
| HEX | `CreateHexFile=0`；目前不自動產生 Intel HEX |
| Linker | RO base=`0x00000000`、RW base=`0x20000000`，entry=`Reset_Handler` |
| Layout | `umfTarg=0`、`useFile=0`、`ScatterFile` 空白，非自訂 scatter |

Linker 另含 `--map --first='startup_m460.o(RESET)' --datacompressor=off --info=inline --entry Reset_Handler`。
現存 [linker response](obj/HSUSBD_HID_Transfer.lnp) 可供核對實際 invocation，並非新的建置證明。
不要把某一次的編譯耗時、code size 或 RAM 使用量寫成固定保證。

### Device 描述與實際配置分開看

| 區域 | XML 描述範圍 | 大小／用途 |
| --- | --- | --- |
| IROM／APROM | `0x00000000..0x0007FFFF` | 512 KiB Device 描述 |
| IRAM | `0x20000000..0x2001FFFF` | **128 KiB**，不是 64 KiB |

目前 linker 以 RO／RW base 配置，既有 response 未列出上述區域的最大長度。
因此需檢查新建置 map 的 RO／RW／ZI、stack／heap；XML 容量不等於已強制檢查的上限。
專案沒有在同一 target 再連結一份 LDROM bootloader。
XML `CLOCK(84000000)` 只是描述；[main.c](../main.c) 的 `SYS_Init()` 實際設定核心 200 MHz。

## 3. 實際納入的來源

- [main.c](../main.c)：應用入口、初始化與 USB command dispatcher。
- [g_def.h](../g_def.h)：位址／偏移，屬 include 定義。
- [descriptors.c](../descriptors.c)、[hid_transfer.c](../hid_transfer.c)、[hid_transfer.h](../hid_transfer.h)：HID。
- [cpld_fan.c](../cpld_fan.c)：CPLD、風扇鏡像、溫度與 EEPROM。
- [nvme_mi.c](../nvme_mi.c)：快取輪詢、live MCTP／NVMe-MI 與 RX IRQ。
- [usb_sn.c](../usb_sn.c)、[log_dump.c](../log_dump.c)：UID serial 與 UART log。
- [xsvftool-m031.c](../xsvftool-m031.c)、[play.c](../play.c)、[svf.c](../svf.c)、[xsvf.c](../xsvf.c)、[tap.c](../tap.c) 等：JTAG player。
- [startup_m460.s](../../../../Library/Device/Nuvoton/m460/Source/ARM/startup_m460.s) 與本地 CMSIS／StdDriver：啟動及周邊。

[main_mctp_test.c](../main_mctp_test.c) 未列入主專案，不是另一個會一起連結的 `main()`。
也不要因檔名含 esp／gpio 就假定該參考 JTAG backend 已被採用。
startup 的 weak `USCI0_IRQHandler` 由 [nvme_mi.c](../nvme_mi.c) 的 strong 定義覆蓋。
建置時若缺此來源，不能把落入 weak handler 誤判成 slave IRQ 正常工作。

## 4. 產物與 After Build

目前 output basename 與檔名如下；這是建置流程，不代表各檔案此刻均已存在：

```text
obj/HSUSBD_HID_Transfer.axf          ELF 格式執行／除錯映像
obj/HSUSBD_HID_Transfer.bin          fromelf 轉出的 raw binary
obj/HSUSBD_HID_Transfer_SUM.bin      FILESUM 封裝映像
lst/HSUSBD_HID_Transfer.map          linker map（建置產物）
```

AXF 是 ELF 格式但副檔名為 AXF；不能杜撰另外自動生成的 ELF 檔名。
HEX 需另行啟用輸出才會產生，不是目前預設產物。
兩個 After Build user program 均啟用，專案中的實際命令為：

```text
fromelf --bin ".\obj\@L.axf" --output ".\obj\@L.bin"
.\FILESUM ".\obj\@L.bin" ".\obj\@L_SUM.bin"
```

`@L` 依 µVision output name 展開；先轉 binary，再用兩個檔案參數封裝。
錯誤時檢查 working directory、工具路徑、輸入存在與新輸出時間，不能只看 callback exit code。
日期版 SUM 檔不是這兩個 callback 的自動輸出，勿混入待發布的舊映像。

### FILESUM 的精確格式

實作在 [FILESUM.cpp](../../../../../app/FILESUM/FILESUM/FILESUM.cpp)，不是 checksum 查詢命令。
需要 argc=3：程式名稱、raw 輸入檔、SUM 輸出檔。
`APROM_SIZE=256*1024`，填充 byte 為 `0xFF`，SUM 固定 262144 bytes。
raw 的實際長度／AXF 大小仍由此次 build 決定，不提供猜測的固定 code size。

| SUM offset | 內容 |
| --- | --- |
| `0..payload end` | 原始 raw bytes，未使用部分填 `FF` |
| `0x3FFF8..0x3FFFB` | 原始 byte 長度，little-endian 32-bit |
| `0x3FFFC..0x3FFFF` | 原始 bytes 加總 `& 0xFFFF`，以 LE32 存放 |

校驗值不是密碼學雜湊，高 16 bits 為零；不要用舊文件的雜湊範例驗證 FILESUM。
payload 必須留出尾端 8 bytes；工具沒有可靠的超長輸入防護，亦可能在錯誤時回傳 0。
因此交付前獨立核對 raw 長度、SUM 長度、padding、LE 長度與 checksum，保留 log。

### APROM 與 ISP 不可混淆

Device APROM 描述為 512 KiB，但 FILESUM 目前封裝只採 256 KiB。
[ISP main.c](../../../ISP/ISP_HID_20/main.c) 使用相同 256 KiB 契約與尾端長度／校驗值。
[ISP_HID_20.uvprojx](../../../ISP/ISP_HID_20/Keil/ISP_HID_20.uvprojx) 是獨立 target，RO base=`0x0F100000`。
主專案 RO base=0；更新 main application 不會自動重新建置／安裝 ISP。
先核對 bootloader、映像種類與容量；AXF、raw BIN、SUM BIN 不應隨意替換。
主機更新 target token 為 `UPDATE M463`，版本查詢為 `Version MCU`。

## 5. Nu-Link 燒錄前檢查

1. 連接正確供電的板與 Nu-Link，確認 SWD／reset、電壓及 target Device。
2. 在 **Options for Target → Debug／Utilities** 核對 Nu-Link driver 與 Settings。
3. 在燒錄設定核對當前 Flash algorithm、APROM／LDROM、範圍與 Erase／Program／Verify。
4. 現存 [Nu_Link_Driver.ini](Nu_Link_Driver.ini) 的 `[M460]` 為 APROM、Bank=0、`M460_AP_256.FLM`。
5. 該區段 Erase／Program／Verify／ResetAndRun 均為 1；不應誤稱目前已選 512 KiB algorithm。
6. XML Utilities 使用 `Bin\Nu_Link.dll`；INI 也不等於已連線、已確認算法可覆蓋本映像。
7. 特別核對 512 KiB Device 描述與 256 KiB algorithm／ISP 契約差異，再使用 **Download**。
8. 燒錄後查看 verify 結果，重啟並核對 USB、版本與 UART；勿靠檔案名稱判定版本。

下載會改寫 flash；不知道 bootloader／Config 狀態時，不要盲目 erase 全片。
進入除錯可在 `main()`／`SYS_Init()` 設 breakpoint；不要長時間停在 ISR 觀察實際時序。
UART3 使用 PB14 RX、PB15 TX，115200 8N1；這不是 USB CDC COM port。

## 6. 上板快速核對

| 項目 | 目前原始碼設定 |
| --- | --- |
| I2C0 | PB4 SDA／PB5 SCL，100 kHz |
| I2C1 | PB0 SDA／PB1 SCL，100 kHz；bank0 NVMe |
| I2C2 | PA10 SDA／PA11 SCL，100 kHz；條件式初始化 |
| UI2C0 | PB13 SDA／PB12 SCL，**10 kHz 診斷設定**；兩線 I²C |
| I2C4 | PF4 SDA／PF5 SCL，100 kHz slave 鏡像；PC14 控制啟閉 |
| JTAG | PA7 TCK／PA6 TMS／PC0 TDI／PC1 TDO |
| HWM／GPIO | selector 40=PC14；PA9 只是 `HWM_SEL` 巨集；selector 48=PB6 |
| USB | VID/PID `0416/502A`；Interrupt `81 IN / 02 OUT`，沒有 Bulk／EP3 |
| packet | EP0=64；HS 資料端點=1024，FS=64；FS 主機流程仍需實測 |
| FW version | B0 回 `26 10 20 01`，不要與 app 版本或發布日期混用 |

完整 7-bit／8-bit 位址表與 D 系列 selector 見 [fw 概覽](../../../../README.md)。
直接 USB D0／D2／D4 `[2]` 帶 8-bit 位址，韌體 `>>1`；bus `0=I2C0`、`1=I2C2`。
VPD Write E0 的 `[11]` 是 carried count、`[12..]` 是資料，最多 180 bytes；不在最後 USB byte。
ID1 是快取，ID9 不支援；不能宣稱 2..19 全部均實作。
ID1 每 bank 快取容量 24；global index 串接兩 bank，不是全域固定 24 槽。
live mux 對 TCA 仍用 PCA 的長度 3 檢查，TCA 卻只有一個位址；local≥8 存在越界風險。

## 7. 腳本與清理

[build_keil_projects.bat](../../../../build_keil_projects.bat) 由目前目錄遞迴建置所有 uvprojx／uvproj，含 ISP。
找 UV4 的兩個既定 Windows 路徑，執行 `-b`、`-j0`，log 為專案檔名加 `_build.log`。
0 印 SUCCESS、1 印 WARNING、>1 印 FAILED；沒有彙總失敗 exit code，必須逐份看 log。
[rename_firmware.py](../../../../rename_firmware.py) 搜尋第一個 `APROM_SUM.bin`／`LDROM.bin` 後複製到目前目錄。
主專案 basename 不匹配這兩個來源名稱，不能宣稱 build 後已自動發布。
命名是 `msi_gpu_cp_<yy_mm_dd>_00_00_07.bin`／`msi_gpu_ld_<yy_mm_dd>_00_00_01.bin`，日期取執行當天。

**清理前備份：腳本沒有刪除前確認，起點都是目前目錄。**
[fw_clean_all.bat](../../../../fw_clean_all.bat) 整刪 `obj / lst / Objects / Listings / RTE`，BIN 也不保留。
[fw_keep_bin.bat](../../../../fw_keep_bin.bat) 整刪 `lst / Listings / RTE`；`obj / Objects` 只留該層 `.bin`。
後者亦刪所有子目錄，包括內含的 BIN，以及 AXF／HEX／map 等非 BIN 檔案。

## 8. 驗證範圍與安全限制

相關主機 mock 測試：[USCI polling](../../../../tests/test_usci_i2c_polling.py)、[slot cache](../../../../tests/test_nvme_slot_cache.py)、[MCTP IRQ](../../../../tests/test_mctp_usci_irq.py)。
它們驗證抽取的 production 分支／sequencer，不是 USB 實機或完整韌體建置測試。
syntax／preprocessor 檢查不能證明 link 成功、RAM 足夠或 flash algorithm 正確。
master polling 關 IRQ；成功 TX 且 STOP 完成後才啟用 slave `0x10` RX，結束再停用。
ISR fault 優先；合法 ACK|STOP 先存最後 byte 再 publish；queue 16 槽可用 15、每槽 128 bytes。
sticky `-10/-11/-12` 分別為 slave fault／overflow／TX failure；`-2` RX timeout 是軟體 loop 預算。
pending STO 不取消、master arbitration lost 不送 STOP；listen 不應清掉新的 response flags。
上板測 bank1 ISR 應用唯讀 live ID6，不用 cache ID1；核對 mux／槽數後才操作。
UI2C0 降速只是 pull-up／rise-time 診斷假說；要用波形、電壓與負載驗證，不推定已證實硬體故障。
Format ID19 需 Dword1=`0x464D5421`（FMT!）；Dword0 含 SES，NSID 固定全 namespaces。
會破壞資料；timeout、cache ID1 或 SMART 成功均不能證明格式化／清除已完成，勿自動重送。
本地 BSP、CMSIS 與 Lib(X)SVF 各有自己的來源標頭；保留各 copyright／授權，不宣告全案統一授權。
