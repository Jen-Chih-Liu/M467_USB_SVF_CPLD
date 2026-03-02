

import subprocess
import json
import os

# 設定執行檔路徑與預設參數
M463_BIN = "M460.exe"  # 請確保此檔案有執行權限 (chmod +x M463)
DEFAULT_COUNT = "0"  # PDF [cite: 21] 提到 Count=1 代表 BPB1

def run_command(description, args):
    """
    執行 M463 指令並印出結果
    """
    # 組合完整指令: ./M463 <Count> <Args...>
    cmd_list = [M463_BIN, DEFAULT_COUNT] + args
    cmd_str = " ".join(cmd_list)
    
    print(f"\n{'='*60}")
    print(f"測試項目: {description}")
    print(f"執行指令: {cmd_str}")
    print(f"{'-'*60}")

    try:
        # 呼叫系統指令
        result = subprocess.run(
            cmd_list,
            capture_output=True,
            text=True,
            check=False # 即使 return code != 0 也不拋出例外，以便查看錯誤訊息
        )

        # 檢查是否有錯誤輸出 (stderr)
        if result.stderr:
            print(f"[stderr] 錯誤輸出:\n{result.stderr.strip()}")

        # 處理標準輸出 (stdout)
        output = result.stdout.strip()
        if output:
            print(f"[stdout] 原始回傳值:\n{output}")
            
            # 嘗試解析 JSON (因為 PDF 顯示回傳多為 JSON 格式)
            try:
                json_data = json.loads(output)
                print(f"[JSON] 解析成功:")
                print(json.dumps(json_data, indent=4, ensure_ascii=False))
            except json.JSONDecodeError:
                print("[Info] 回傳值不是標準 JSON 格式")
        else:
            print("[Info] 無回傳值")

    except FileNotFoundError:
        print(f"[Error] 找不到執行檔 {M463_BIN}，請確認路徑或編譯是否成功。")
    except Exception as e:
        print(f"[Error] 執行發生例外: {e}")

def main():
    # 檢查執行檔是否存在
    if not os.path.exists(M463_BIN):
        print(f"警告: 找不到 {M463_BIN}，僅列出將被執行的指令供參考...\n")
    
    # 定義測試案例列表 (依據 PDF 文件順序)
    test_cases = [
        # 1. CPLD Id 
        ("CPLD Id", ["CPLD", "Id"]),
        
        # 2. CPLD Version [cite: 38]
        ("CPLD Version", ["CPLD", "Version"]),
        
        # 3. CPLD Get (讀取 Register 範例) [cite: 46]
        ("CPLD Get Reg", ["CPLD", "Get", "0x00", "3"]),
        
        # 3. CPLD Set (寫入 Register 範例) [cite: 51]
        ("CPLD Set Reg", ["CPLD", "Set", "0x60", "0xaa"]),
        
        # 4. HDD Info [cite: 59]
        ("HDD Info", ["HDD", "Info"]),
        
        # 5. HDD Port Status [cite: 70]
        ("HDD Port Status", ["HDD", "Port", "status"]),
        
        # 6. HDD LED Status [cite: 79]
        ("HDD LED Status", ["HDD", "LED", "Status"]),
        
        # 7. HDD Temp [cite: 91]
        ("HDD Temp", ["HDD", "Temp"]),
        
        # 8. HDD BasicCmd (範例: identify_device) [cite: 104]
        #("HDD BasicCmd Identify", ["HDD", "BasicCmd", "identify_device"]),
        
        # 9-1. NVMe-Mi Get FW Revision [cite: 123]
        #("NVMe-Mi Get FW Revision", ["HDD", "NVMe-Mi", "Get", "FW", "Revision"]),
        
        # 9-2. NVMe-Mi Get Health Status Alerts [cite: 134]
        #("NVMe-Mi Health Alerts", ["HDD", "NVMe-Mi", "Get", "Health", "Status", "Alerts"]),
        
        # 9-6. NVMe-Mi Get Health Info [cite: 177]
        #("NVMe-Mi Health Info", ["HDD", "NVMe-Mi", "Get", "Health", "Info"]),
        
        # 10. Update (需要假檔案，這裡僅生成指令) [cite: 186]
        #("Update Firmware (Simulated)", ["Update", "M463", "firmware.bin"]),
        
        # 11. Version MCU [cite: 197]
        ("Version MCU", ["Version", "MCU"]),
        
        # 12. BPB Sensor Get [cite: 202]
        ("BPB Sensor Get", ["BPB", "Sensor", "Get"]),
        
        
        # 13. Reset [cite: 205]

        ("Reset System", ["Reset", "getval"]),
        ("Reset System", ["Reset", "setval", "0xa9"]),
        ("Reset System", ["Reset", "getval"]),
        #("Reset System", ["Reset"]),  #need sleep 500ms
        #("Reset System", ["Reset", "getvar"]),
        
        # 14. I2CBYPASS (範例) [cite: 209]
        ("I2C Bypass Detect", ["I2CBYPASS", "get", "0xf0", "0x0", "3"]),
        ("I2C Bypass Detect", ["I2CBYPASS", "set", "0xf0", "0x60", "0x55"]),
        
        # 15. FAN RPM Get [cite: 213]
        ("FAN RPM Get", ["FAN", "RPM", "Get"]),
        
        # 16. FAN DUTY Get [cite: 219]
        ("FAN Duty Get", ["FAN", "Duty", "Get"]),

        ("Golbal led status", ["global", "LED", "STATUS"]),

        ("dump all", ["dumpall"]),
    ]

    # 迴圈執行所有測試案例
    for desc, args in test_cases:
        run_command(desc, args)

if __name__ == "__main__":
    main()