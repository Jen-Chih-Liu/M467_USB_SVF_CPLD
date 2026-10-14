#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NVMe-MI (0xE0 NVMEMI) 專用測試腳本

透過 M460.exe 的 NVMEMI 指令，對指定 NVMe 磁碟槽逐一執行已實作的
NVMe-MI 命令，並解析回傳的 JSON。目前韌體已實作的 CommandID：
    1  Basic Management                 (SMBus block read 快取)
    4  Read NVMe-MI Data Structure      (Opcode 0x00)
    6  Configuration Get                (Opcode 0x04)
    7  VPD Read                         (Opcode 0x05)
    10 NVM Subsystem Health Status Poll (Opcode 0x01)
    11 Controller Health Status Poll    (Opcode 0x02)
    12 Health Info + Alerts             (Opcode 0x01)
    14 FW Revision   (Admin Get Log FW Slot,  LID 0x03)
    17 SMART Log     (Admin Get Log SMART,     LID 0x02)
    18 Device Identify (Admin Identify Controller, CNS 0x01)

對 14/17/18 Admin passthrough 回應會額外解析帶出人可讀欄位摘要。
    python test_nvmemi.py --index 2       # 指定 NVMe 槽 2（1-based）
    python test_nvmemi.py --usb 0 --index 1
    python test_nvmemi.py --only 4,7,11   # 只跑指定 CommandID
"""

import argparse
import json
import os
import re
import subprocess
import sys

# --- 執行檔位置 ---------------------------------------------------------------
# 預設抓同 repo 的 Debug 版 M460.exe；找不到時退回 PATH 中的 "M460.exe"。
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_BIN = os.path.join(_THIS_DIR, "M460", "build", "Debug", "M460.exe")
M460_BIN = _DEFAULT_BIN if os.path.exists(_DEFAULT_BIN) else "M460.exe"


# --- 測試案例 -----------------------------------------------------------------
# 每一筆：(CommandID, 說明, [Dword0, Dword1] 額外參數字串清單)
#   - 額外參數以字串傳入，支援 0x-hex（strtoul base 0）。
#   - 空清單代表使用韌體端預設值。
def build_test_cases(index):
    idx = str(index)
    return [
        # 1 Basic Management：讀 bmc_report 快取，最穩定，當健全性檢查基準
        (1, "Basic Management", [idx]),

        # 2 Get Endpoint UUID：MCTP Control 0x03，回傳 16-byte RFC4122 UUID
        (2, "Get Endpoint UUID (MCTP Ctrl 0x03)",         [idx]),

        # 3 Get Message Type Support：MCTP Control 0x05，回傳支援的訊息類型清單
        (3, "Get Msg Type Support (MCTP Ctrl 0x05)",      [idx]),

        # 4 Read NVMe-MI Data Structure：DTYP 放在 Dword0[31:24]
        #    DTYP=3 (Controller Info) 需帶有效 Controller ID，改由主流程從
        #    DTYP=2 (Controller List) 動態解析後串接執行，故此處不列靜態案例。
        (4, "Read MI Data - NVM Subsystem Info (DTYP=0)", [idx, "0x00000000"]),
        (4, "Read MI Data - Port Info (DTYP=1)",          [idx, "0x01000000"]),
        (4, "Read MI Data - Controller List (DTYP=2)",    [idx, "0x02000000"]),

        # 6 Configuration Get：Dword0[7:0]=Config ID, [31:24]=Port ID
        (6, "Config Get - SMBus Freq (CID=1, Port=1)",    [idx, "0x01000001"]),

        # 7 VPD Read：Dword0[15:0]=DOFST, Dword1[7:0]=DLEN
        (7, "VPD Read - offset 0, len 0x20",              [idx, "0x00000000", "0x20"]),
        (7, "VPD Read - offset 0, len 0x60",              [idx, "0x00000000", "0x60"]),

        # 10 NVM Subsystem Health Status Poll：Dword1 bit31=Clear Status（此處不清）
        (10, "NVM Subsystem Health Status Poll",          [idx]),

        # 12 Health Info + Alerts：Opcode 0x01，回應含 Composite Controller
        #    Status（alert 旗標）+ 溫度 + 壽命。Dword1 bit31=Clear（此處不清）
        (12, "Health Info + Alerts",                      [idx]),

        # 11 Controller Health Status Poll：Dword0=0 時韌體套用預設 0x87010000
        (11, "Controller Health Status Poll (default)",   [idx]),

        # 13 Power Management：Admin Get Features (opcode 0x0A, FID 0x02, SEL=0)
        #    無資料傳輸，結果在 CQE DW0：PS[4:0], WH[7:5]（韌體回傳 4-byte DW0）
        (13, "Power Management (Get Feature FID 0x02)",    [idx]),

        # 15 Health Alerts / 16 Health Info：與 10/12 同為 NVM Subsystem Health
        #    Status Poll (Opcode 0x01)，韌體以不同 CommandID 標籤回傳同結構。
        (15, "Health Alerts (NVM Subsystem Health Poll)", [idx]),
        (16, "Health Info (NVM Subsystem Health Poll)",   [idx]),

        # --- Admin passthrough（NMIMT=2h，回應可能較大，Dword0 當作 byte offset 分頁）---
        # 14 FW Revision：Get Log Page FW Slot Info (LID 0x03, 64B)
        #    FW 版本字串在回傳資料的 data[8..15]（8 個 ASCII）
        (14, "FW Revision (Get Log FW Slot, offset 0)",   [idx, "0x00000000"]),

        # 17 SMART Log：Get Log Page SMART/Health (LID 0x02, 512B)
        #    首 200B 已含關鍵欄位（Critical Warning/溫度/Spare/Percentage Used...）
        (17, "SMART Log (offset 0, first 200B)",          [idx, "0x00000000"]),
        (17, "SMART Log (offset 200, next window)",       [idx, "0x000000C8"]),

        # 18 Device Identify：Identify Controller (CNS 0x01, 4096B)
        #    首 72B 含 VID/SSVID/SN/MN/FR，最有用的識別資訊
        (18, "Device Identify (offset 0, first 200B)",    [idx, "0x00000000"]),
    ]


def build_write_cases(index):
    """寫入 / 破壞性 CommandID 的測試案例。

    這些指令會改變或抹除磁碟狀態，故「不」納入預設批次；只有在 --only 明確
    指定對應 CommandID 時才會執行，且 8 (VPD Write) / 19 (Format NVM) 另需
    加上 --danger 旗標二次確認。回傳 dict: cmd_id -> (desc, extra_args)。
    """
    idx = str(index)
    return {
        # 5 Configuration Set：SMBus/I2C Frequency (Config ID 0x01)
        #    DWORD0 [07:00]=CID=01h, [11:08]=Freq(1=100kHz,2=400kHz,3=1MHz),
        #    [31:24]=Port ID。此處把 Port 1 設為 100kHz（多數磁碟的預設值），
        #    若磁碟本來就跑 100kHz 即為無害的 no-op。**注意：改動 SMBus 頻率
        #    可能中斷 MCTP 通訊，請確認欲設定的值正確。**
        #    dword0 = 0x01000101 → CID=01, Freq=1(100kHz), Port=1
        5: ("Configuration Set - SMBus Freq 100kHz (CID=1, Port=1)",
            [idx, "0x01000101"]),

        # 8 VPD Write：Dword0=DOFST，其後為資料位元組。破壞性（改寫 VPD/FRU）。
        #    語法：NVMEMI 8 <idx> <DOFST> <byte0> <byte1> ...
        #    範例在 offset 0 寫入單一位元組 0x00（請務必先用 CmdID 7 備份原值）。
        8: ("VPD Write (offset 0, 1 byte)", [idx, "0x00000000", "0x00"]),

        # 19 Format NVM：Admin opcode 0x80。**極度破壞性：抹除所有使用者資料**。
        #    Dword0=CDW10，Dword1 必須=0x464D5421 ("FMT!") 才會執行。
        #    CDW10 欄位配置（NVMe Base Spec）：
        #      bits[3:0]  = LBAF (LBA Format index)
        #      bit[4]     = MSET (Metadata Settings)
        #      bits[7:5]  = PI   (Protection Information)
        #      bit[8]     = PIL  (Protection Information Location)
        #      bits[10:9] = SES  (Secure Erase Settings) ← 注意是 2-bit 欄位，
        #                   從 bit9 開始，不是 bit8！之前誤用 0x100(bit8=PIL)
        #                   導致 SES 仍為 0，等同沒有要求安全抹除。
        #    SES 數值意義：
        #      0 = No secure erase requested（只重建/套用 LBA Format，規格不保證
        #          抹除既有使用者資料，很多控制器只是重置 FTL mapping table，
        #          實體 flash 內容可能原封不動——這是格式化回報 Success 後，
        #          之前寫入的測試檔仍「看得到/讀得到」的根本原因）。CDW10=0x00000000
        #      1 = User Data Erase（規格要求保證清除所有使用者資料，這裡預設用這個，
        #          依磁碟容量可能耗時數秒至數分鐘）。CDW10=0x00000200 (bit9=1)
        #      2 = Cryptographic Erase（直接銷毀 AES 金鑰，毫秒級完成；僅限控制器
        #          Identify 的 FNA bit2=1 才支援）。CDW10=0x00000400 (bit10=1)
        #    dword0 = 0x00000200 → LBAF=0, SES=1 (User Data Erase)
        #    注意：SES=1 的實際抹除可能耗時遠超過 SES=0 的「假格式化」，韌體若在
        #    retry 逾時前仍未收到磁碟回應，會回報 Status="InProgress"（非失敗），
        #    此時請改用 CmdID 1 / 17 輪詢，直到該指令回報 Status="Success" 為止。
        19: ("Format NVM (SES=1 User Data Erase, 需確認碼)",
             [idx, "0x00000200", "0x464D5421"]),
    }


def run_setup(usb_index):
    """執行 NVMe-MI 測試前的前置設定。

    先執行 `M460.exe <usb> GPIO Setval 40 0`，把 GPIO 40 設為 0（啟用 NVMe
    I2C 匯流排 / HWM_SEL 相關腳位），確保後續 MCTP 交換能正常存取磁碟。
    """
    cmd_list = [M460_BIN, str(usb_index), "GPIO", "Setval", "40", "0"]
    cmd_str = " ".join(cmd_list)
    print(f"\n[前置設定] {cmd_str}")
    try:
        result = subprocess.run(
            cmd_list,
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        print(f"  -> 找不到執行檔 {M460_BIN}")
        return
    except Exception as e:  # noqa: BLE001
        print(f"  -> 執行例外: {e}")
        return

    stdout = result.stdout.strip()
    stderr = result.stderr.strip()
    if stderr:
        print(f"  [stderr] {stderr}")
    if stdout:
        print(f"  [stdout] {stdout}")


def run_nvmemi(usb_index, cmd_id, extra_args):
    """執行單一 NVMEMI 指令，回傳 (cmd_str, parsed_json_or_None, raw_stdout, stderr)。"""
    cmd_list = [M460_BIN, str(usb_index), "NVMEMI", str(cmd_id)] + extra_args
    cmd_str = " ".join(cmd_list)

    try:
        result = subprocess.run(
            cmd_list,
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        return cmd_str, None, "", f"找不到執行檔 {M460_BIN}"
    except Exception as e:  # noqa: BLE001
        return cmd_str, None, "", f"執行例外: {e}"

    stdout = result.stdout.strip()
    stderr = result.stderr.strip()

    # M460.exe 常在 JSON 前印一行 "updated usb table, ..."，需擷取 JSON 主體。
    parsed = None
    brace = stdout.find("{")
    if brace >= 0:
        try:
            parsed = json.loads(stdout[brace:])
        except json.JSONDecodeError:
            parsed = None

    return cmd_str, parsed, stdout, stderr


def annotate_reason(reason):
    """替韌體回傳的錯誤字串補上 completion / status code 的人類可讀語意。"""
    if not reason:
        return reason
    # MCTP 基礎規範 Completion Code（CmdID 2/3 走 MCTP Control）
    mctp_cc = {
        0x00: "Success", 0x01: "Error (Generic)",
        0x02: "Error (Invalid Data)", 0x03: "Error (Invalid Length)",
        0x04: "Error (Not Ready)", 0x05: "Unsupported Command",
    }
    m = re.search(r"Completion 0x([0-9A-Fa-f]{2})", reason)
    if m:
        cc = int(m.group(1), 16)
        note = mctp_cc.get(cc, "Command-Specific/Vendor" if cc >= 0x80 else "Unknown")
        return f"{reason}  ({note})"
    # NVMe-MI Status（native/admin 路徑）
    m = re.search(r"Status 0x([0-9A-Fa-f]{2})", reason)
    if m:
        st = int(m.group(1), 16)
        nvme_st = {
            0x00: "Success", 0x01: "More Processing Required",
            0x02: "Internal Error", 0x03: "Invalid Command Opcode",
            0x04: "Invalid Parameter", 0x05: "Invalid Command Size",
            0x06: "Invalid Command Input Data Size", 0x07: "Access Denied",
        }
        return f"{reason}  ({nvme_st.get(st, 'Unknown')})"
    return reason


def summarize(parsed):
    """由解析後的 JSON 取出簡短狀態字串。"""
    if not isinstance(parsed, dict):
        return "PARSE-FAIL", ""
    status = parsed.get("Status", "?")
    data = parsed.get("Data", {})
    if status == "Success":
        count = data.get("Count", "?")
        raw = data.get("Raw data", "")
        return "OK", f"Count={count}  Raw={raw}"
    reason = data.get("Error", "") if isinstance(data, dict) else ""
    return "FAIL", annotate_reason(reason)


def parse_raw_bytes(parsed):
    """把成功回應的 "Raw data" ("00,07,...") 轉成整數清單；失敗回傳 []。"""
    if not isinstance(parsed, dict):
        return []
    if parsed.get("Status") != "Success":
        return []
    raw = parsed.get("Data", {}).get("Raw data", "")
    out = []
    for tok in raw.split(","):
        tok = tok.strip()
        if not tok:
            continue
        try:
            out.append(int(tok, 16))
        except ValueError:
            return []
    return out


def extract_first_controller_id(parsed):
    """從 DTYP=2 (Controller List) 回應解析第一個 Controller ID。

    回應 payload = Mgmt Response DWORD[23:0] (3 bytes) + Controller List：
        [3..4] = NUMIDS (LE, 識別子數量)
        [5..6] = 第一個 Controller Identifier (LE)
    找不到有效 ID 時回傳 None。
    """
    b = parse_raw_bytes(parsed)
    if len(b) < 7:
        return None
    numids = b[3] | (b[4] << 8)
    if numids < 1:
        return None
    return b[5] | (b[6] << 8)


# --- Admin 回應欄位解析 -------------------------------------------------------
def _ascii(b, start, end):
    """取 byte 清單 [start, end) 轉可列印 ASCII，去除尾端空白/NUL。"""
    s = "".join(chr(x) if 32 <= x < 127 else "." for x in b[start:end])
    return s.rstrip("\x00").rstrip()


def _le(b, start, width):
    """從 byte 清單的 start 起取 width 位組小端整數（支援 128-bit 計數器）。"""
    v = 0
    for i in range(width):
        if start + i < len(b):
            v |= b[start + i] << (8 * i)
    return v


def _extra_offset(extra):
    """從 extra 參數取 Dword0（分頁 byte offset）。extra=[idx, dword0, ...]。"""
    if len(extra) >= 2:
        try:
            return int(extra[1], 0)
        except ValueError:
            return 0
    return 0


def decode_endpoint_uuid(b):
    """CmdID 2：Get Endpoint UUID (MCTP Ctrl 0x03)。回傳 16-byte RFC4122 UUID。"""
    if len(b) < 16:
        return []
    u = "".join(f"{x:02X}" for x in b[:16])
    pretty = f"{u[0:8]}-{u[8:12]}-{u[12:16]}-{u[16:20]}-{u[20:32]}"
    return [f"Endpoint UUID          = {pretty}"]


def decode_msg_type_support(b):
    """CmdID 3：Get Message Type Support (MCTP Ctrl 0x05)。byte0=數量，後接類型清單。"""
    if len(b) < 1:
        return []
    names = {
        0x00: "MCTP Control", 0x01: "PLDM", 0x02: "NCSI", 0x03: "Ethernet",
        0x04: "NVMe-MI", 0x05: "SPDM", 0x06: "Secured Msg (SPDM)",
    }
    cnt = b[0]
    lines = [f"Supported Msg Types    = {cnt}"]
    for t in range(cnt):
        if 1 + t >= len(b):
            break
        mt = b[1 + t]
        lines.append(f"  [{t}] 0x{mt:02X} => {names.get(mt, 'Vendor/Unknown')}")
    return lines


def decode_health_poll(b):
    """CmdID 10/12/15/16：NVM Subsystem Health Status Poll (Opcode 0x01)。

    回應 payload = Mgmt Response DWORD(3B) + NVM Subsystem Health Data Structure：
        [3]=NSS(NVM Subsystem Status)  [4]=SW(Smart Warnings, 反向邏輯: 1=健康)
        [5]=CTEMP(Composite Temperature, °C 有號)  [6]=PDLU(Percentage Drive Life Used)
        [7..8]=CCS(Composite Controller Status)
    視韌體回傳長度而定，欄位不足時則略過。"""
    # SW 位元反向：bit=1 代表該警告未觸發（健康），故實際告警 = (~SW)。
    sw_bits = {
        0x01: "Spare 低於門檻", 0x02: "溫度超標", 0x04: "可靠度降級",
        0x08: "唯讀模式", 0x10: "備援記憶體失效",
    }
    lines = []
    if len(b) >= 4:
        nss = b[3]
        lines.append(f"NVM Subsystem Status   = 0x{nss:02X}"
                     + (" (Drive Functional)" if (nss & 0x20) else ""))
    if len(b) >= 5:
        sw = b[4]
        active = (~sw) & 0xFF  # 反向：0 位元 = 告警觸發
        if active == 0:
            lines.append(f"Smart Warnings         = 0x{sw:02X} (無告警)")
        else:
            flags = ", ".join(n for m, n in sw_bits.items() if active & m)
            lines.append(f"Smart Warnings         = 0x{sw:02X} (有告警: {flags})")
    if len(b) >= 6:
        ctemp = b[5]
        if ctemp >= 0x80:
            ctemp -= 0x100  # 有號攝氏度
        lines.append(f"Composite Temperature  = {ctemp} °C")
    if len(b) >= 7:
        lines.append(f"Percentage Drive Used  = {b[6]} %")
    if len(b) >= 9:
        ccs = b[7] | (b[8] << 8)
        lines.append(f"Composite Ctrl Status  = 0x{ccs:04X}")
    return lines


def decode_power_mgmt(b):
    """CmdID 13：Get Features Power Management。回傳 4-byte CQE DW0 (LE)：
        PS(Power State)  = bits [4:0]
        WH(Workload Hint)= bits [7:5]
    """
    if len(b) < 4:
        return []
    dw0 = _le(b, 0, 4)
    ps = dw0 & 0x1F
    wh = (dw0 >> 5) & 0x07
    return [
        f"CQE DWORD0             = 0x{dw0:08X}",
        f"Power State (PS)       = {ps}",
        f"Workload Hint (WH)     = {wh}",
    ]


def decode_fw_revision(b):
    """CmdID 14：Get Log FW Slot Info (LID 0x03)。byte0=AFI，FRS1..7 各 8B ASCII。"""
    lines = []
    if len(b) < 8:
        return lines
    afi = b[0]
    lines.append(f"Active Firmware Slot   = {afi & 0x07} (AFI=0x{afi:02X})")
    for slot in range(7):
        s0 = 8 + slot * 8
        if s0 + 8 > len(b):
            break
        if not any(b[s0:s0 + 8]):
            continue  # 全 0x00 代表該槽未寫入韌體，略過
        frs = _ascii(b, s0, s0 + 8)
        if frs:
            lines.append(f'FW Slot {slot + 1} Revision    = "{frs}"')
    return lines


def decode_smart(b, offset):
    """CmdID 17：SMART / Health Log (LID 0x02)。依分頁 offset 解析不同欄位。"""
    lines = []
    if offset == 0:
        if len(b) < 6:
            return lines
        cw = b[0]
        temp_k = _le(b, 1, 2)
        cw_txt = " (無告警)" if cw == 0 else " (有告警!)"
        lines.append(f"Critical Warning       = 0x{cw:02X}{cw_txt}")
        lines.append(f"Composite Temperature  = {temp_k} K ({temp_k - 273} °C)")
        lines.append(f"Available Spare        = {b[3]} %")
        lines.append(f"Available Spare Thresh = {b[4]} %")
        lines.append(f"Percentage Used        = {b[5]} %")
        if len(b) >= 48:
            dur = _le(b, 32, 16)
            lines.append(f"Data Units Read        = {dur} ({dur * 1000 * 512 / 1e9:.2f} GB)")
        if len(b) >= 64:
            duw = _le(b, 48, 16)
            lines.append(f"Data Units Written     = {duw} ({duw * 1000 * 512 / 1e9:.2f} GB)")
        if len(b) >= 80:
            lines.append(f"Host Read Commands     = {_le(b, 64, 16)}")
        if len(b) >= 96:
            lines.append(f"Host Write Commands    = {_le(b, 80, 16)}")
        if len(b) >= 128:
            lines.append(f"Power Cycles           = {_le(b, 112, 16)}")
        if len(b) >= 144:
            lines.append(f"Power On Hours         = {_le(b, 128, 16)}")
        if len(b) >= 160:
            lines.append(f"Unsafe Shutdowns       = {_le(b, 144, 16)}")
        if len(b) >= 176:
            lines.append(f"Media & Integrity Err  = {_le(b, 160, 16)}")
        if len(b) >= 192:
            lines.append(f"Error Log Entries      = {_le(b, 176, 16)}")
    elif offset == 200:
        # 溫度感測器 1/2 位於 SMART 絕對位移 200/202，故此視窗相對 0/2。
        if len(b) >= 2:
            t1 = _le(b, 0, 2)
            if t1:
                lines.append(f"Temperature Sensor 1   = {t1} K ({t1 - 273} °C)")
        if len(b) >= 4:
            t2 = _le(b, 2, 2)
            if t2:
                lines.append(f"Temperature Sensor 2   = {t2} K ({t2 - 273} °C)")
    return lines


def decode_identify_controller(b):
    """CmdID 18：Identify Controller (CNS 0x01)。首 84B 含 VID/SN/MN/FR/CNTLID/VER。"""
    lines = []
    if len(b) < 72:
        return lines
    vid = _le(b, 0, 2)
    ssvid = _le(b, 2, 2)
    lines.append(f"PCI Vendor ID (VID)    = 0x{vid:04X}"
                 + (" (Samsung)" if vid == 0x144D else ""))
    lines.append(f"Subsystem Vendor SSVID = 0x{ssvid:04X}")
    lines.append(f'Serial Number (SN)     = "{_ascii(b, 4, 24)}"')
    lines.append(f'Model Number (MN)      = "{_ascii(b, 24, 64)}"')
    lines.append(f'Firmware Revision (FR) = "{_ascii(b, 64, 72)}"')
    if len(b) >= 76:
        lines.append(f"IEEE OUI               = {b[75]:02X}{b[74]:02X}{b[73]:02X}")
    if len(b) >= 78:
        lines.append(f"MDTS                   = {b[77]}")
    if len(b) >= 80:
        lines.append(f"Controller ID (CNTLID) = 0x{_le(b, 78, 2):04X}")
    if len(b) >= 84:
        ver = _le(b, 80, 4)
        lines.append(f"NVMe Version (VER)     = {(ver >> 16) & 0xFFFF}."
                     f"{(ver >> 8) & 0xFF}.{ver & 0xFF} (0x{ver:08X})")
    return lines


def decode_fields(cmd_id, extra, parsed):
    """依 CommandID 與分頁 offset 解析 Admin 回應，回傳人可讀欄位字串清單。"""
    b = parse_raw_bytes(parsed)
    if cmd_id == 5:
        return ["Configuration Set 已接受（NVMe-MI Status = Success）"]
    if cmd_id == 8:
        return ["VPD Write 已接受（NVMe-MI Status = Success）"]
    if cmd_id == 19:
        sc = b[0] if b else 0
        return [f"Format NVM 完成（NVMe Status Code = 0x{sc:02X}）"]
    if not b:
        return []
    offset = _extra_offset(extra)
    if cmd_id == 2:
        return decode_endpoint_uuid(b)
    if cmd_id == 3:
        return decode_msg_type_support(b)
    if cmd_id in (10, 12, 15, 16):
        return decode_health_poll(b)
    if cmd_id == 13:
        return decode_power_mgmt(b)
    if cmd_id == 14:
        return decode_fw_revision(b)
    if cmd_id == 17:
        return decode_smart(b, offset)
    if cmd_id == 18 and offset == 0:
        return decode_identify_controller(b)
    return []


def main():
    ap = argparse.ArgumentParser(description="NVMe-MI (NVMEMI) 測試腳本")
    ap.add_argument("--usb", type=int, default=0, help="USB 裝置索引 (預設 0)")
    ap.add_argument("--index", type=int, default=1, help="NVMe 磁碟槽索引 (1-based，預設 1)")
    ap.add_argument("--only", type=str, default="",
                    help="只跑指定 CommandID，逗號分隔，例如 4,7,11")
    ap.add_argument("--bin", type=str, default="", help="覆寫 M460.exe 路徑")
    ap.add_argument("--danger", action="store_true",
                    help="允許執行破壞性指令 8 (VPD Write) / 19 (Format NVM)")
    args = ap.parse_args()

    global M460_BIN
    if args.bin:
        M460_BIN = args.bin

    only = set()
    if args.only:
        for tok in args.only.split(","):
            tok = tok.strip()
            if tok.isdigit():
                only.add(int(tok))

    if not os.path.exists(M460_BIN) and os.sep in M460_BIN:
        print(f"警告: 找不到 {M460_BIN}，仍會嘗試執行（可能在 PATH 中）。\n")

    print("=" * 66)
    print(f"NVMe-MI 測試   BIN={M460_BIN}")
    print(f"USBIndex={args.usb}  NVMEIndex={args.index}"
          + (f"  Only={sorted(only)}" if only else ""))
    print("=" * 66)

    cases = build_test_cases(args.index)
    results = []
    controller_id = None  # 由 DTYP=2 (Controller List) 動態解析

    # 前置設定：GPIO Setval 40 0
    run_setup(args.usb)

    for cmd_id, desc, extra in cases:
        if only and cmd_id not in only:
            continue

        cmd_str, parsed, stdout, stderr = run_nvmemi(args.usb, cmd_id, extra)
        tag, detail = summarize(parsed)
        results.append((cmd_id, desc, tag))

        print(f"\n[CmdID {cmd_id:2d}] {desc}")
        print(f"  $ {cmd_str}")
        if stderr:
            print(f"  [stderr] {stderr}")
        if parsed is not None:
            print(f"  -> {tag}  {detail}")
            # Admin 回應（CmdID 14/17/18）額外列出解析後的欄位摘要。
            if tag == "OK":
                for fl in decode_fields(cmd_id, extra, parsed):
                    print(f"       · {fl}")
        else:
            # 解析失敗時印原始輸出協助除錯
            print(f"  -> {tag}")
            if stdout:
                print(f"  [stdout] {stdout}")

        # 擷取 Controller List (DTYP=2) 的第一個 Controller ID，供後續串接。
        if cmd_id == 4 and "0x02000000" in extra and tag == "OK":
            controller_id = extract_first_controller_id(parsed)
            if controller_id is not None:
                print(f"  [chain] 解析到 Controller ID = 0x{controller_id:04X}")
            else:
                print("  [chain] 無法從 Controller List 解析出 Controller ID")

    # --- 串接：Read MI Data - Controller Info (DTYP=3) ----------------------
    # DTYP=3 需要有效的 Controller ID（Dword0[15:0]），使用上面 DTYP=2 解析
    # 出的值動態帶入，Dword0 = 0x03000000 | controller_id。
    if (not only) or (4 in only):
        if controller_id is not None:
            d0 = 0x03000000 | (controller_id & 0xFFFF)
            desc = f"Read MI Data - Controller Info (DTYP=3, CTRLID=0x{controller_id:04X})"
            cmd_str, parsed, stdout, stderr = run_nvmemi(
                args.usb, 4, [str(args.index), f"0x{d0:08X}"])
            tag, detail = summarize(parsed)
            results.append((4, desc, tag))

            print(f"\n[CmdID  4] {desc}")
            print(f"  $ {cmd_str}")
            if stderr:
                print(f"  [stderr] {stderr}")
            if parsed is not None:
                print(f"  -> {tag}  {detail}")
            else:
                print(f"  -> {tag}")
                if stdout:
                    print(f"  [stdout] {stdout}")
        else:
            print("\n[CmdID  4] Read MI Data - Controller Info (DTYP=3)")
            print("  -> SKIP  未取得 Controller ID（DTYP=2 未成功或未執行）")
            results.append((4, "Read MI Data - Controller Info (DTYP=3, skipped)", "SKIP"))

    # --- 寫入 / 破壞性指令（5/8/19）-------------------------------------
    # 這些指令預設不執行；僅當 --only 明確列出對應 CommandID 時才跑。
    # 8 (VPD Write) / 19 (Format NVM) 另需 --danger 旗標二次確認。
    if only:
        write_cases = build_write_cases(args.index)
        for wcmd in (5, 8, 19):
            if wcmd not in only:
                continue
            desc, extra = write_cases[wcmd]
            if wcmd in (8, 19) and not args.danger:
                print(f"\n[CmdID {wcmd:2d}] {desc}")
                print("  -> SKIP  破壞性指令，需加 --danger 旗標才會執行")
                results.append((wcmd, desc + " (skipped, 需 --danger)", "SKIP"))
                continue

            cmd_str, parsed, stdout, stderr = run_nvmemi(args.usb, wcmd, extra)
            tag, detail = summarize(parsed)
            results.append((wcmd, desc, tag))

            print(f"\n[CmdID {wcmd:2d}] {desc}")
            print(f"  $ {cmd_str}")
            if stderr:
                print(f"  [stderr] {stderr}")
            if parsed is not None:
                print(f"  -> {tag}  {detail}")
                if tag == "OK":
                    for fl in decode_fields(wcmd, extra, parsed):
                        print(f"       · {fl}")
            else:
                print(f"  -> {tag}")
                if stdout:
                    print(f"  [stdout] {stdout}")

    # --- 總結 ---------------------------------------------------------------
    print("\n" + "=" * 66)
    print("測試總結")
    print("-" * 66)
    ok = sum(1 for _, _, t in results if t == "OK")
    for cmd_id, desc, tag in results:
        mark = "PASS" if tag == "OK" else tag
        print(f"  CmdID {cmd_id:2d}  {mark:10s}  {desc}")
    print("-" * 66)
    print(f"通過 {ok} / {len(results)}")
    print("=" * 66)

    # 有任何非 OK 就以非零離開碼結束（方便 CI 判斷）
    return 0 if ok == len(results) and results else 1


if __name__ == "__main__":
    sys.exit(main())
