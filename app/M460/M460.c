#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(_WIN32) || defined(_WIN64)
#include "libusb.h"
#else
// On Linux, pkg-config sometimes automatically points the include path to the libusb-1.0 folder.
// To be safe, you can use __has_include check (C++17) or write it directly.
#include <libusb-1.0/libusb.h>
#endif

#include "config.h"
// ---------------------------------------------------------
// 1. Basic Definitions and Helpers
// ---------------------------------------------------------

// Cross-platform case-insensitive comparison
#ifdef _WIN32
#define STR_EQUAL(a, b) (_stricmp((a), (b)) == 0)
#else
#include <strings.h>
#define STR_EQUAL(a, b) (strcasecmp((a), (b)) == 0)
#endif

#include "cJSON.h"
#define DUTY_TO_REG8(_DUTY_)  (uint8_t)((((uint32_t)(_DUTY_) * 255UL) + 50UL) / 100UL)

// Define Function Pointer type
// count: The <Count> parameter from the command line
// argc, argv: Remaining arguments passed to the Handler for sub-command processing (e.g., "Id", "Version")
typedef int (*cmd_handler_t)(int count, int argc, char* argv[]);

// Define command table structure
typedef struct {
    const char* name;       // Command name (e.g., "CPLD", "HDD")
    cmd_handler_t handler;  // Corresponding handler function
    const char* help;       // Help text
} cmd_entry_t;

// ---------------------------------------------------------
// 2. Implementation of Specific Handlers
// ---------------------------------------------------------

// --- Group A: CPLD Related Processing ---
int handle_cpld(int count, int argc, char* argv[]) {
    // argv[0] is "CPLD" (context passed from main)
    // argv[1] is expected to be a sub-command (Id, Version, Get, Set)

    if (argc < 2) {
        dbg_printf("[Error] CPLD command needs sub-command (Id, Version, Get, Set)\n");
        return -1;
    }
#if 0
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        dbg_printf("read_multi false\n");
        return -1;
    }
#endif
    const char* subCmd = argv[1];
    
    if (STR_EQUAL(subCmd, "Id")) {
        cmd_printf("[CMD] Count=%d, Action=Read CPLD ID\n", count);
        M463_BoardData_t boardInfo;
        memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
        int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
        if (ret != 0)
        {
            // Prepare JSON
            cJSON* root = cJSON_CreateObject();

            // Dynamically generate Key: "CPLD ID"
            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "CPLD ID");

            // Fill in result (Success/fail)
            cJSON_AddStringToObject(root, key_buf, "fail, no supports port");
            

            // Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
            return -1;
        }

        

        // Prepare a buffer to store the converted string
        char id_str_buf[16];

        // Format hex value as string
        // %02x : Output "2a" (padded with 0 if < 16, e.g., 0x0a -> "0a")
        // 0x%x : Output "0x2a"
        snprintf(id_str_buf, sizeof(id_str_buf), "0x%x", boardInfo.jtag_id);

        // --- cJSON Generation ---
        cJSON* root = cJSON_CreateObject();

        // Note: Use AddString because Hex must be a string in JSON
        cJSON_AddStringToObject(root, "CPLD_ID", id_str_buf);

        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        // Release memory
        free(out);
        cJSON_Delete(root);

         }

    else if (STR_EQUAL(subCmd, "Version")) {
        cmd_printf("[CMD] Count=%d, Action=Read CPLD Version\n", count);

        M463_BoardData_t boardInfo;
        memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
        int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
        if (ret != 0)
        {
            // Prepare JSON
            cJSON* root = cJSON_CreateObject();

            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "CPLD Version");

            // Fill in result (Success/fail)
            cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


            // Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
            return -1;
        }

        // Prepare a buffer to store the converted string
        char version_str_buf[16];

        // Format hex value as string
        snprintf(version_str_buf, sizeof(version_str_buf), "0x%02x", boardInfo.cpld_version);

        // --- cJSON Generation ---
        cJSON* root = cJSON_CreateObject();

        // Note: Use AddString because Hex must be a string in JSON
        cJSON_AddStringToObject(root, "CPLD_Version", version_str_buf);

        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        // Release memory
        free(out);
        cJSON_Delete(root);

    }
    else if (STR_EQUAL(subCmd, "Get")) {
        cmd_printf("[CMD] Count=%d, Action=CPLD Get value\n", count);
        // 2. Parse Reg and Len
        // Use strtoul to support hex strings starting with 0x
        unsigned int reg_addr = strtoul(argv[2], NULL, 16);
        int length = atoi(argv[3]);

        if (length <= 0) {
            dbg_printf("[Error] Length must be > 0\n");
            return -1;
        }

        unsigned char pi2c_buf[32] = { 0 };
        if (usbd_multi_cpld_i2c_get((unsigned char)count, reg_addr, length, &pi2c_buf[0]) == 0)
        {
            // 3. Prepare JSON objects
            cJSON* root = cJSON_CreateObject();      // Outer layer { }
            cJSON* data_obj = cJSON_CreateObject();  // Inner layer { "data1":... }

            // 4. Fill JSON with read data
            char key_buf[32];   // Temporary buffer for "data1", "data2"...
            char val_buf[32];   // Temporary buffer for "0x10"...

            for (int i = 0; i < length; i++) {
                // Generate Key: "data1", "data2"... (note i+1)
                snprintf(key_buf, sizeof(key_buf), "data%d", i + 1);

                // Generate Value: "0x10"
                snprintf(val_buf, sizeof(val_buf), "0x%02x", pi2c_buf[i]);

                // Add to inner object
                cJSON_AddStringToObject(data_obj, key_buf, val_buf);
            }

            // 5. Generate outer Key: "Get_0x10"
            char root_key_buf[32];
            snprintf(root_key_buf, sizeof(root_key_buf), "Get_0x%x", reg_addr);

            // 6. Mount inner object to outer layer
            // Structure becomes: { "Get_0x10": { ...data_obj... } }
            cJSON_AddItemToObject(root, root_key_buf, data_obj);

            // 7. Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root); // Deleting root automatically releases data_obj
        }
        else {
            cJSON* root = cJSON_CreateObject();

            // Dynamically generate Key
            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "CPLD_GET_0x%x", reg_addr);

           cJSON_AddStringToObject(root, key_buf, "fail");
     
            // Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
        
        }
    }
    else if (STR_EQUAL(subCmd, "Set")) {
        // Subsequent parameters might follow: M463 <cnt> CPLD Set <Reg> <Val>
        cmd_printf("[CMD] Count=%d, Action=CPLD Set value\n", count);

        // 1. Check argument count
        if (argc < 4) {
            dbg_printf("[Error] Usage: CPLD Set <Reg> <Data>\n");
            return -1;
        }

        // 2. Parse input parameters (Reg and Data)
        // strtoul supports 0x prefix for hex
        unsigned int reg_addr = strtoul(argv[2], NULL, 16);
        unsigned int write_val = strtoul(argv[3], NULL, 16);
        int is_success;
        if (usbd_multi_cpld_i2c_set((unsigned char)count,reg_addr, write_val) == 0){
            // 3. Execute hardware write
             is_success = 1;
    }
    else
    {
        is_success = 0;
    }
        // 4. Prepare JSON
        cJSON* root = cJSON_CreateObject();

        // 5. Dynamically generate Key: "CPLD_Set_0x10"
        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "CPLD_Set_0x%x", reg_addr);

        // 6. Fill in result (Success/fail)
        if (is_success) {
            cJSON_AddStringToObject(root, key_buf, "Success");
        }
        else {
            cJSON_AddStringToObject(root, key_buf, "fail");
        }

        // 7. Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);

    }
    else {
        dbg_printf("[Error] Unknown CPLD sub-command: %s\n", subCmd);
        return -1;
    }
    return 0;
}


// Define a structure to simulate hardware-read NVMe information
typedef struct {
    const char* product;
    const char* sn;
    const char* version;
} nvme_info_t;

// Mock data for 4 drives
nvme_info_t mock_nvme_data[] = {
    {"Phison", "AB123", "02.22"}, // NVME1
    {"Phison", "AB124", "01.22"}, // NVME2
    {"Intel",  "AB125", "02.12"}, // NVME3
    {"NA",     "NA",    "NA"}     // NVME4
};


void trim_trailing_spaces(char* str) {
    int len= (int)strlen(str);
    while (len > 0 && str[len - 1] == ' ') {
        str[len - 1] = '\0';
        len--;
    }
}

// ---------------------------------------------------------
// Helper: Determine vendor name based on Vendor ID (VID)
// ---------------------------------------------------------
const char* get_vendor_name(uint16_t vid) {
    switch (vid) {
    case 0x8086: return "Intel";
    case 0x144D: return "Samsung";
    case 0x1987: return "Phison";
    case 0x15B7: return "Sandisk"; // WD
    case 0x1179: return "Kioxia";  // Toshiba
    case 0xC0A9: return "Micron";
    case 0x10EC: return "Realtek";
    case 0x1D97: return "Hoodisk";
    case 0x0000:
    case 0xFFFF: return "NA";      // Empty slot or invalid
    default:     return "Unknown";
    }
}


// [HDD Info] - Corrected version (matching NVMe-MI Basic Management format)
int do_hdd_info(int count, int argc, char* argv[]) {

    // 1. Read USB data
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo));

    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        // Prepare JSON
        cJSON* root = cJSON_CreateObject();

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "hdd info");

        // Fill in result (Success/fail)
        cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


        // Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
        return -1;
    }

    // 2. Create JSON root object
    cJSON* root = cJSON_CreateObject();
    int total_slots = boardInfo.hdd_amount;
    cJSON_AddNumberToObject(root, "NVMECount", total_slots);

    // 3. Prepare pointer array
    uint8_t* slot_ptrs[] = {
        boardInfo.nvme_slot_0,
        boardInfo.nvme_slot_1,
        boardInfo.nvme_slot_2,
        boardInfo.nvme_slot_3,
        boardInfo.nvme_slot_4,
        boardInfo.nvme_slot_5,
        boardInfo.nvme_slot_6,
        boardInfo.nvme_slot_7,
        boardInfo.nvme_slot_8,
        boardInfo.nvme_slot_9,
        boardInfo.nvme_slot_10,
        boardInfo.nvme_slot_11,
        boardInfo.nvme_slot_12,
        boardInfo.nvme_slot_13,
        boardInfo.nvme_slot_14,
        boardInfo.nvme_slot_15,
    };

    char key_buf[16];
    char sn_buf[21];   // SN max 20 + null

    // 4. Iterate through each slot and parse
    for (int i = 0; i < total_slots; i++) {
        uint8_t* raw_data = slot_ptrs[i];
        cJSON* drive_obj = cJSON_CreateObject();

        // ---------------------------------------------------------
        // [Fix 1] Parse VID (Offset 9 & 10, Big-Endian)
        // ---------------------------------------------------------
        uint16_t vid = (raw_data[9] << 8) | raw_data[10];
        const char* vendor_name = get_vendor_name(vid);

        // If VID is 0 or FFFF, treat as no device
        if (vid == 0x0000 || vid == 0xFFFF) {
            cJSON_AddStringToObject(drive_obj, "product", "NA");
            cJSON_AddStringToObject(drive_obj, "SN", "NA");
            cJSON_AddStringToObject(drive_obj, "Version", "NA");
        }
        else {
            cJSON_AddStringToObject(drive_obj, "product", vendor_name);

            // ---------------------------------------------------------
            // [Fix 2] Parse SN (Offset 11, Length 20)
            // ---------------------------------------------------------
            memcpy(sn_buf, &raw_data[11], 20);
            sn_buf[20] = '\0';            // Null terminate
            trim_trailing_spaces(sn_buf); // Remove trailing spaces
            cJSON_AddStringToObject(drive_obj, "SN", sn_buf);

            // ---------------------------------------------------------
            // [Fix 3] Version Handling
            // ---------------------------------------------------------
            // In NVMe-MI Basic Command Response (32 bytes), 
            // Firmware Version is not included (Byte 31 is PEC).
            // Currently displaying "NA".

            cJSON_AddStringToObject(drive_obj, "Version", "NA");
        }

        // Add to JSON
        snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);
        cJSON_AddItemToObject(root, key_buf, drive_obj);
    }

    // 5. Print and release
    char* out = cJSON_Print(root);
    dbg_printf("%s\n", out);
    free(out);
    cJSON_Delete(root);

    return 0;
}


// [HDD Port Status] - Added JSON implementation
int do_hdd_port(int count, int argc, char* argv[]) {
    // argv[0]="Port", argv[1]="Status"
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);

    if (ret != 0)
    {
        // Prepare JSON
        cJSON* root = cJSON_CreateObject();

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "HDD PORT STATUS");

        // Fill in result (Success/fail)
        cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


        // Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
        return -1;
    }


    if (argc < 2 || !STR_EQUAL(argv[1], "Status")) {
        dbg_printf("[Error] Usage: HDD Port Status\n");
        return -1;
    }
    cmd_printf("[CMD] Count=%d, Action=HDD PORT Status\n", count);
    
    cJSON* root = cJSON_CreateObject();
    // Use the read hard drive count
    int hdd_amount = boardInfo.hdd_amount;
    cJSON_AddNumberToObject(root, "NVMECount", hdd_amount);

    char key_buf[16];

    // 4. Parse register status (Core logic)
    for (int i = 0; i < hdd_amount; i++) {
        // [Step A] Calculate corresponding Byte Index
        // 0-3 at index 0 (0x40), 4-7 at index 1 (0x41)...
        int reg_index = i / 4;

        // [Step B] Calculate corresponding Bit Shift
        // i%4=0 -> shift 0, i%4=1 -> shift 2...
        int bit_shift = (i % 4) * 2;

        // [Step C] Get the Byte (corresponding to cpld_reg_40_4f in struct)
        // Ensure i/4 doesn't exceed array bounds (0-15)
        uint8_t raw_byte = boardInfo.cpld_reg_40_4f[reg_index];

        // [Step D] Extract 2 bits status
        // Shift right, then mask with 0x03 (binary 11) to get last two bits
        uint8_t status_bits = (raw_byte >> bit_shift) & 0x03;

        // [Step E] Determine status (based on definition)
        // 01(1): SATA Present, 11(3): NVMe Present -> Plugin detected
        // 00(0): Not Present,  10(2): NVMe Not Present -> No plugin
        const char* status_str = "false";

        if (status_bits == 0x01 || status_bits == 0x03) {
            status_str = "true";
        }

        // [Step F] Add to JSON
        snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);
        cJSON_AddStringToObject(root, key_buf, status_str);
    }

    char* out = cJSON_Print(root);
    json_printf("%s\n", out);
    free(out);
    cJSON_Delete(root);

    return 0;
}

// [HDD LED Status] - Read Register and parse LED status
int do_hdd_led(int count, int argc, char* argv[]) {

    // 1. Check arguments
    if (argc < 2 || !STR_EQUAL(argv[1], "Status")) {
        dbg_printf("[Error] Usage: HDD LED Status\n");
        return -1;
    }

    // 2. Read USB data
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo));

    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        // Prepare JSON
        cJSON* root = cJSON_CreateObject();

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "HDD LED STATUS");

        // Fill in result (Success/fail)
        cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


        // Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
        return -1;
    }

    cmd_printf("[CMD] Count=%d, Action=HDD LED Status (Real Data)\n", count);

    // 3. Create JSON
    cJSON* root = cJSON_CreateObject();

    // Use read hard drive count (Offset 0x4)
    int hdd_amount = boardInfo.hdd_amount;
    cJSON_AddNumberToObject(root, "NVMECount", hdd_amount);

    char key_buf[16];

    // Status string mapping array
    // 00: Standby, 01: Fault, 10: Locate, 11: Rebuild
    const char* status_map[] = {
        "Standby", // 00 (Value 0)
        "Fault",   // 01 (Value 1)
        "Locate",  // 10 (Value 2)
        "Rebuild"  // 11 (Value 3)
    };

    // 4. Parse register status
    for (int i = 0; i < hdd_amount; i++) {
        // [Step A] Calculate corresponding Byte Index
        int reg_index = i / 4;

        // [Step B] Calculate corresponding Bit Shift
        int bit_shift = (i % 4) * 2;

        // [Step C] Get the Byte
        uint8_t raw_byte = boardInfo.cpld_reg_40_4f[reg_index];

        // [Step D] Extract 2 bits status
        uint8_t status_bits = (raw_byte >> bit_shift) & 0x03;

        if (status_bits == 0x01 || status_bits == 0x03) {
            //////////////////////////////////////////////////////////////
            // [Step A] Calculate Byte Index (starting from 0x50)
            int reg_index = i / 4;

            // [Step B] Calculate Bit Shift
            int bit_shift = (i % 4) * 2;

            // [Step C] Get the Byte (note use of cpld_reg_50_5f)
            uint8_t raw_byte = boardInfo.cpld_reg_50_5f[reg_index];

            // [Step D] Extract 2 bits status
            uint8_t status_val = (raw_byte >> bit_shift) & 0x03;

            // [Step E] Add to JSON
            snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);

            // Use status_val (0~3) as index for lookup
            cJSON_AddStringToObject(root, key_buf, status_map[status_val]);
        }
        else {
            // [Step E] Add to JSON
            snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);
            cJSON_AddStringToObject(root, key_buf, "NA");
        }
    }

    // 5. Output and release
    char* out = cJSON_Print(root);
    json_printf("%s\n", out);
    free(out);
    cJSON_Delete(root);

    return 0;
}

// [HDD Temp]
int do_hdd_temp(int count, int argc, char* argv[]) {
    cmd_printf("[CMD] Count=%d, Action=Read HDD Temp\n", count);
    
    // 1. Read USB data
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo));

    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        // Prepare JSON
        cJSON* root = cJSON_CreateObject();

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "hdd read temp");

        // Fill in result (Success/fail)
        cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


        // Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
        return -1;
    }
    int total_slots = boardInfo.hdd_amount;;
    uint8_t* slot_ptrs[] = {
     boardInfo.nvme_slot_0,
     boardInfo.nvme_slot_1,
     boardInfo.nvme_slot_2,
     boardInfo.nvme_slot_3,
     boardInfo.nvme_slot_4,
     boardInfo.nvme_slot_5,
     boardInfo.nvme_slot_6,
     boardInfo.nvme_slot_7,
             boardInfo.nvme_slot_8,
        boardInfo.nvme_slot_9,
        boardInfo.nvme_slot_10,
        boardInfo.nvme_slot_11,
        boardInfo.nvme_slot_12,
        boardInfo.nvme_slot_13,
        boardInfo.nvme_slot_14,
        boardInfo.nvme_slot_15,
    };

    // 2. Create JSON object
    cJSON* root = cJSON_CreateObject();

    // 3. Add NVMECount
    cJSON_AddNumberToObject(root, "NVMECount", total_slots);

    // 4. Loop to generate temperature info for NVME1 ~ NVME6
    char key_buf[32];
    char val_buf[32];   

    for (int i = 0; i < total_slots; i++) {
        // [Step A] Calculate Byte Index
        int reg_index = i / 4;

        // [Step B] Calculate Bit Shift
        int bit_shift = (i % 4) * 2;

        // [Step C] Get the Byte
        uint8_t raw_byte = boardInfo.cpld_reg_40_4f[reg_index];

        // [Step D] Extract 2 bits status
        uint8_t status_bits = (raw_byte >> bit_shift) & 0x03;

        if (status_bits == 0x01 || status_bits == 0x03) {

            /////////////////////////////////////////////////
            uint8_t* raw_data = slot_ptrs[i];
            // Dynamically generate Key: NVME1, NVME2...
            snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);

            // Byte 3: Composite Temperature (CTemp)
            uint8_t temp_raw = raw_data[3];
            
            snprintf(val_buf, sizeof(val_buf), "%d", (int8_t)temp_raw);

            // Add to inner object
            cJSON_AddStringToObject(root, key_buf, val_buf);
        }
        else
        {
            snprintf(key_buf, sizeof(key_buf), "NVME%d", i + 1);
            cJSON_AddStringToObject(root, key_buf, "NA");

        }
        
    }

    // 5. Print and release
    char* out = cJSON_Print(root);
    json_printf("%s\n", out);

    free(out);
    cJSON_Delete(root);

    return 0;
}

// [HDD NVMe-Mi]
int do_hdd_nvme(int count, int argc, char* argv[]) {
    char* cmd = (argc > 1) ? argv[1] : "Default";
    cmd_printf("[CMD] Count=%d, Action=NVMe-Mi Send: %s\n", count, cmd);
    return 0;
}

// ---------------------------------------------------------
// 3. Create HDD-specific Lookup Table (Local Table)
// ---------------------------------------------------------
cmd_entry_t hdd_table[] = {
    {"Info",     do_hdd_info,  "Show HDD Info JSON"},
    {"Port",     do_hdd_port,  "Port Status JSON"},
    {"LED",      do_hdd_led,   "LED Status"},
    {"Temp",     do_hdd_temp,  "Temperature"},
    {"NVMe-Mi",  do_hdd_nvme,  "NVMe-Mi <Cmd>"},
    {NULL, NULL, NULL}
};

int handle_hdd(int count, int argc, char* argv[]) {
    /*
     * Input: ./M463 1 HDD Info
     * handle_hdd receives argv[0]="HDD", argv[1]="Info"
     */

    if (argc < 2) {
        dbg_printf("[Error] HDD command needs sub-command:\n");
        for (int i = 0; hdd_table[i].name != NULL; i++) {
            dbg_printf("  - %s\n", hdd_table[i].name);
        }
        return -1;
    }

    char* sub_cmd = argv[1];
    int found = 0;

    for (int i = 0; hdd_table[i].name != NULL; i++) {
        if (STR_EQUAL(sub_cmd, hdd_table[i].name)) {
            // Key: Shift pointer forward by 1 (argv + 1)
            // So in do_hdd_port, argv[0] becomes "Port", argv[1] is "Status"
            hdd_table[i].handler(count, argc - 1, argv + 1);
            found = 1;
            break;
        }
    }

    if (!found) {
        dbg_printf("[Error] Unknown HDD sub-command: %s\n", sub_cmd);
        return -1;
    }

    return 0;
}


// Helper Function to convert ISP_STATE to readable string
const char* isp_state_to_string(int state) {
    switch (state) {
    case RES_PASS:           return "Success";
    case RES_FALSE:          return "General Fail";
    case RES_FILE_NO_FOUND:  return "File Not Found";
    case RES_PROGRAM_FALSE:  return "Programming Fail";
    case RES_CONNECT:        return "Connected";
    case RES_CONNECT_FALSE:  return "USB Connection Fail";
    case RES_CHIP_FOUND:     return "Chip Found";
    case RES_DISCONNECT:     return "Disconnected";
    case RES_FILE_LOAD:      return "File Loaded";
    case RES_FILE_SIZE_OVER: return "File Size Overflow";
    case RES_TIME_OUT:       return "Operation Timeout";
    case RES_SN_OK:          return "SN Handshake OK";
    case RES_DETECT_MCU:     return "MCU Detected";
    case RES_NO_DETECT:      return "No Device Detected";
    default:                 return "Unknown Error Code";
    }
}

// --- Group C: Single Commands ---
int handle_update(int count, int argc, char* argv[]) {
    cmd_printf("[CMD] Count=%d, Action=Update Target: %s\n", count, argv[1]);

    // 1. Argument check
    if (argc < 3) {
        dbg_printf("[Error] Usage: Update <M463/CPLD> <File>\n");
        return -1;
    }

    char* target = argv[1];
    char* filepath = argv[2];
    int isp_result = RES_FALSE; // Default to fail
    char fail_reason[128] = { 0 }; // Initialize buffer
    int result = 0;
    // 2. Validate Target
    if (!STR_EQUAL(target, "M463") && !STR_EQUAL(target, "CPLD")) {
        dbg_printf("[Error] Unknown target: %s (Must be M463 or CPLD)\n", target);
        return -1;
    }

    // 3. Execute ISP process
    if (STR_EQUAL(target, "M463"))
    {
        // Check file extension
        if (is_bin_file(filepath) != 0) {
            isp_result = RES_FILE_NO_FOUND;
            snprintf(fail_reason, sizeof(fail_reason), "Invalid File Extension");
            goto create_json; // Jump to JSON generation
        }

        // Switch to LDROM
        if (usbd_multi_mcu_jumper_ldrom(count) != 0)
        {
            cJSON* root = cJSON_CreateObject();

            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "UPDATE M463");

            // Fill in result (Success/fail)
            cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


            // Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
            return -1;
        }

        // --- Core modification: receive return value ---
        isp_result = isp_mcu(filepath);

    create_json:
        // 4. Create JSON response
        cJSON* root = cJSON_CreateObject();

        // Check if isp_result is RES_PASS
        if (isp_result == RES_PASS) {
            // Success format: {"Update Task": "Success"}
            cJSON_AddStringToObject(root, "Update Task", "Success");
        }
        else {
            // Fail format: {"Update Task": "Fail,Reason"}
            if (strlen(fail_reason) == 0) {
                snprintf(fail_reason, sizeof(fail_reason), "%s", isp_state_to_string(isp_result));
            }

            char fail_msg[128];
            // CSV style string: "Fail,Timeout" or "Fail,File Not Found"
            snprintf(fail_msg, sizeof(fail_msg), "Fail,%s", fail_reason);
            cJSON_AddStringToObject(root, "Update Task", fail_msg);

            // FAE Debug suggestion: also print to Console
            dbg_printf("[Error] ISP Failed Code: %d (%s)\n", isp_result, fail_reason);
        }

        // 5. Print and release
        char* out = cJSON_PrintUnformatted(root); 
        dbg_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
    }

    else if (STR_EQUAL(target, "CPLD"))
    {
        if (usbd_multi_mcu_stop_bmc_monitor(count) != 0)
        {
            cJSON* root = cJSON_CreateObject();

            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "UPDATE CPLD");

            // Fill in result (Success/fail)
            cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


            // Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
            return -1;
        }
        sleep_seconds(1);
        result = cpld_svf_update(count, filepath, fail_reason, sizeof(fail_reason));
        usbd_multi_mcu_start_bmc_monitor(count);
        cJSON* root = cJSON_CreateObject();

        // Note: original cpld_svf_update returning 0 is CPLD_SUCCESS
        if (result == CPLD_SUCCESS) {
            cJSON_AddStringToObject(root, "Update Task", "Success");
        }
        else {
            if (strlen(fail_reason) == 0) {
                snprintf(fail_reason, sizeof(fail_reason), "%s", cpld_error_to_string(result));
            }

            char fail_msg[256];
            snprintf(fail_msg, sizeof(fail_msg), "Fail,%s", fail_reason);
            cJSON_AddStringToObject(root, "Update Task", fail_msg);
        }
         char* out = cJSON_PrintUnformatted(root);
        dbg_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
    }

    return 0;
}

int handle_version(int count, int argc, char* argv[]) {
    // M463 <Count> Version <MCU>
    cmd_printf("[CMD] Count=%d, Action=Read MCU Version\n", count);
    // 1. Argument check
    if (argc < 2) {
        printf("[Error] Usage: Version <Target> (e.g., MCU)\n");
        return -1;
    }

    // 2. Check if Target is MCU
    if (STR_EQUAL(argv[1], "MCU")) {
        uint32_t mcu_version = 0;
        if (usb_read_multi_mcu_version(count, &mcu_version) != 0)
        {
            
                // Prepare JSON
                cJSON* root = cJSON_CreateObject();

                char key_buf[64];
                snprintf(key_buf, sizeof(key_buf), "MCU Version");

                // Fill in result (Success/fail)
                cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


                // Print and release
                char* out = cJSON_Print(root);
                dbg_printf("%s\n", out);

                free(out);
                cJSON_Delete(root);
                return -1;
            

        }
        // Prepare a buffer to store the converted string
        char version_str_buf[16];

        snprintf(version_str_buf, sizeof(version_str_buf), "0x%08x", mcu_version);

        // --- cJSON Generation ---
        cJSON* root = cJSON_CreateObject();

        // Note: Hex must be a string in JSON
        cJSON_AddStringToObject(root, "MCU_Version", version_str_buf);

        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        // Release memory
        free(out);
        cJSON_Delete(root);

    }
    else {
        dbg_printf("[Error] Unknown version target: %s\n", argv[1]);
        return -1;
    }
    return 0;
}

int handle_bpb(int count, int argc, char* argv[]) {
    // M463 <Count> BPB Sensor Get    
    // Context:
    // argv[0] = "BPB"
    // argv[1] = "Sensor"
    // argv[2] = "Get"

    // 1. Check argument integrity
    if (argc < 3 || !STR_EQUAL(argv[1], "Sensor") || !STR_EQUAL(argv[2], "Get")) {
        dbg_printf("[Error] Usage: BPB Sensor Get\n");
        return -1;
    }
    char bpb_str_buf[16];

    // 2. Read USB data
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo));

    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        // Prepare JSON
        cJSON* root = cJSON_CreateObject();

        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "BPB SENSOR GET");

        // Fill in result (Success/fail)
        cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


        // Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);
        return -1;
    }
    float local_temperature=show_temperature(boardInfo.temp_sensor_high, boardInfo.temp_sensor_low);

    snprintf(bpb_str_buf, sizeof(bpb_str_buf), "%.2f", local_temperature);

    // --- cJSON Generation ---
    cJSON* root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "BPB_Temp", bpb_str_buf);

    char* out = cJSON_Print(root);
    json_printf("%s\n", out);

    // Release memory
    free(out);
    cJSON_Delete(root);

    return 0;
}

int handle_reset(int count, int argc, char* argv[]) {
    cmd_printf("[CMD] Count=%d, Action=System Reset\n", count);
    
    if (argc == 3 && STR_EQUAL(argv[0], "Reset") && STR_EQUAL(argv[1], "Setval")) {
        // Value should be in argv[2]
        unsigned char temp = (unsigned char)strtoul(argv[2], NULL, 16);
        int is_success = 1;
        if (usb_multi_mcu_reset_set_var(count,temp) == 0)
            is_success = 1;
        else
            is_success = 0;
        // 2. Create JSON response
        cJSON* root = cJSON_CreateObject();

        if (is_success) {
            cJSON_AddStringToObject(root, "MCU_RESET", "PASS");
        }
        else {
            cJSON_AddStringToObject(root, "MCU_RESET", "FALSE");
        }

        // 3. Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);

        return 0;
    }

    if (argc == 2 && STR_EQUAL(argv[0], "Reset") && STR_EQUAL(argv[1], "Getval")) {

        uint8_t reset_var = 0;
        if (usb_multi_mcu_reset_get_var(count, &reset_var) != 0)
        {
             // Prepare JSON
            cJSON* root = cJSON_CreateObject();

            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "RESET_VAR");

            // Fill in result (Success/fail)
            cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


            // Print and release
            char* out = cJSON_Print(root);
            dbg_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);
            return -1;


        }
        // Prepare a buffer to store the converted string
        char var_str_buf[16];

        snprintf(var_str_buf, sizeof(var_str_buf), "0x%02x", reset_var);

        // --- cJSON Generation ---
        cJSON* root = cJSON_CreateObject();

        cJSON_AddStringToObject(root, "VALUE", var_str_buf);

        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        // Release memory
        free(out);
        cJSON_Delete(root);

        return 0;
    }
        if (argc == 1 && STR_EQUAL(argv[0], "Reset")) 
    {
        // 1. Execute Reset action
        int is_success = 1;
        if (usb_multi_mcu_reset(count) == 0)
            is_success = 1;
        else
            is_success = 0;
        // 2. Create JSON response
        cJSON* root = cJSON_CreateObject();

        if (is_success) {
            cJSON_AddStringToObject(root, "MCU_RESET", "PASS");
        }
        else {
            cJSON_AddStringToObject(root, "MCU_RESET", "FALSE");
        }

        // 3. Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);

        return 0;
    }
    return -1;
}
extern void list_nuvoton_devices(void);
int handle_i2c_bypass(int count, int argc, char* argv[]) {
    // M463 <Count> I2CBYPASS <Cmd>
    cmd_printf("[CMD] Count=%d, Action=I2C Bypass Mode\n", count);

    if (argc < 2) {
        dbg_printf("[Error] CPLD command needs sub-command (Id, Version, Get, Set)\n");
        return -1;
    }

    const char* subCmd = argv[1];
    
    if (STR_EQUAL(subCmd, "Get")) {
        cmd_printf("[CMD] Count=%d, Action=i2c Get value\n", count);
        // 2. Parse Reg and Len
        unsigned int i2c_addr = strtoul(argv[2], NULL, 16);
        unsigned int reg_addr = strtoul(argv[3], NULL, 16);
        int length = atoi(argv[4]);

        if (length <= 0) {
            dbg_printf("[Error] Length must be > 0\n");
            return -1;
        }

        unsigned char pi2c_buf[32] = { 0 };
        if (usbd_multi_pass_i2c_get((unsigned char)count, i2c_addr,reg_addr, length, &pi2c_buf[0]) == 0)
        {
            // 3. Prepare JSON objects
            cJSON* root = cJSON_CreateObject();      
            cJSON* data_obj = cJSON_CreateObject();  

            // 4. Fill JSON with data
            char key_buf[32];   
            char val_buf[32];   

            for (int i = 0; i < length; i++) {
                // Generate Key
                snprintf(key_buf, sizeof(key_buf), "data%d", i + 1);

                // Generate Value
                snprintf(val_buf, sizeof(val_buf), "0x%02x", pi2c_buf[i]);

                // Add to inner object
                cJSON_AddStringToObject(data_obj, key_buf, val_buf);
            }

            // 5. Generate outer Key
            char root_key_buf[32];
            snprintf(root_key_buf, sizeof(root_key_buf), "Get_0x%x", reg_addr);

            // 6. Mount inner object to outer layer
            cJSON_AddItemToObject(root, root_key_buf, data_obj);

            // 7. Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root); 
        }
        else {
            cJSON* root = cJSON_CreateObject();

            // 5. Dynamically generate Key
            char key_buf[64];
            snprintf(key_buf, sizeof(key_buf), "I2C_PASS_GET_0x%x", reg_addr);

            cJSON_AddStringToObject(root, key_buf, "fail");

            // 7. Print and release
            char* out = cJSON_Print(root);
            json_printf("%s\n", out);

            free(out);
            cJSON_Delete(root);

        }
    }
    else if (STR_EQUAL(subCmd, "Set")) {
        // Possible subsequent parameters: M463 <cnt> i2c Set <Reg> <Val>
        cmd_printf("[CMD] Count=%d, Action=CPLD Set value\n", count);

        // 1. Check argument count
        if (argc < 4) {
            dbg_printf("[Error] Usage: CPLD Set <Reg> <Data>\n");
            return -1;
        }

        // 2. Parse input parameters (Reg and Data)
        unsigned int i2c_addr = strtoul(argv[2], NULL, 16);
        unsigned int reg_addr = strtoul(argv[3], NULL, 16);
        unsigned int write_val = strtoul(argv[4], NULL, 16);
        int is_success;
        if (usbd_multi_pass_i2c_set((unsigned char)count, i2c_addr, reg_addr, write_val) == 0) {
            // 3. Execute write
            is_success = 1;
        }
        else
        {
            is_success = 0;
        }
        // 4. Prepare JSON
        cJSON* root = cJSON_CreateObject();

        // 5. Dynamically generate Key
        char key_buf[64];
        snprintf(key_buf, sizeof(key_buf), "I2C_Set_0x%x", reg_addr);

        // 6. Fill in result
        if (is_success) {
            cJSON_AddStringToObject(root, key_buf, "Success");
        }
        else {
            cJSON_AddStringToObject(root, key_buf, "fail");
        }

        // 7. Print and release
        char* out = cJSON_Print(root);
        json_printf("%s\n", out);

        free(out);
        cJSON_Delete(root);

    }
    else {
        dbg_printf("[Error] Unknown CPLD sub-command: %s\n", subCmd);
        return -1;
    }
    return 0;
}

int handle_usblist(int count, int argc, char* argv[]) {
    // Logic to list USB devices
    cmd_printf("[CMD] Count=%d, Action=List All USB Devices\n", count);

    // If future usblist needs parameters (e.g., usblist -v), use argc/argv
    if (argc > 1 && STR_EQUAL(argv[1], "-v")) {
        dbg_printf("      (Verbose Mode: Show Details...)\n");
    }

    list_nuvoton_devices();

    return 0;
}


int handle_appversion(int count, int argc, char* argv[]) {
    // Logic to list USB devices
    cmd_printf("[CMD] Count=%d, Action=List All USB Devices\n", count);

    if (argc > 1 && STR_EQUAL(argv[1], "-v")) {
        dbg_printf("      (Verbose Mode: Show Details...)\n");
    }

    char* mock_temp = app_version_number;

    // 3. Create JSON object
    cJSON* root = cJSON_CreateObject();

    // 4. Add temperature info
    cJSON_AddStringToObject(root, "app_version", mock_temp);

    // 5. Print and release
    char* out = cJSON_Print(root);
    json_printf("%s\n", out);

    free(out);
    cJSON_Delete(root);

    return 0;
}


int handle_dumpall(int count, int argc, char* argv[]) {
    // Logic to dump all registers
    cmd_printf("[CMD] Count=%d, Action=dump All resgister \n", count);
    M463_BoardData_t boardInfo;
    memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
    int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
    if (ret != 0)
    {
        dbg_printf("read_multi false\n");
        return -1;
    }
    dumpall_printf("temperature sensor read %f celsius\n\r",
        show_temperature(boardInfo.temp_sensor_high, boardInfo.temp_sensor_low));
    
    dumpall_printf("cpld version: 0x%x\n\r", boardInfo.cpld_version);
    dumpall_printf("cpld test version: 0x%x\n\r", boardInfo.cpld_test_ver);
    dumpall_printf("cpld hdd amount: 0x%x\n\r", boardInfo.hdd_amount);
    dumpall_printf("cpld jtag id: 0x%x\n\r", boardInfo.jtag_id);
    for (uint8_t loc = 0; loc < boardInfo.hdd_amount; loc++)
    {
        dumpall_printf("cpld port status register [0x%x]: 0x%x\n\r", cpld_hdd_port_status + loc, boardInfo.cpld_reg_40_4f[loc]);
    }

    for (uint8_t loc = 0; loc < boardInfo.hdd_amount; loc++)
    {
        dumpall_printf("cpld port status register [0x%x]: 0x%x\n\r", cpld_hdd_status + loc, boardInfo.cpld_reg_50_5f[loc]);
    }

    for (uint8_t loc = 0; loc < boardInfo.hdd_amount; loc++)
    {
        dumpall_printf("cpld led register [0x%x]: 0x%x\n\r", cpld_hdd_led + loc, boardInfo.cpld_reg_60_6f[loc]);
    }
    
    dumpall_printf("dump_nvme0\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_0[0]);
    dumpall_printf("dump_nvme1\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_1[0]);
    dumpall_printf("dump_nvme2\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_2[0]);
    dumpall_printf("dump_nvme3\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_3[0]);
    dumpall_printf("dump_nvme4\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_4[0]);
    dumpall_printf("dump_nvme5\n\r");
    print_nvme_basic_management_info(&boardInfo.nvme_slot_5[0]);

    return 0;
}


const char* mock_fan_rpm[] = { "1200", "3600", "na" };
const char* mock_fan_duty[] = { "10",   "36",   "na" };

int handle_fan(int count, int argc, char* argv[]) {
    // Context Check:
    // argv[0] = "FAN" (passed from main table)
    // argv[1] = Sub-category ("RPM" or "Duty")
    // argv[2] = Action ("Get" or "Set")

    if (argc < 3) {
        dbg_printf("[Error] Usage: FAN <RPM/Duty> <Get/Set> [Args...]\n");
        return -1;
    }

    const char* sub_type = argv[1]; // RPM or Duty
    const char* action = argv[2]; // Get or Set

    // =========================================================
    // 1. Process FAN RPM
    // =========================================================
    if (STR_EQUAL(sub_type, "RPM")) {
        if (STR_EQUAL(action, "Get")) {
            M463_BoardData_t boardInfo;
            memset(&boardInfo, 0, sizeof(boardInfo)); // Initialize to zero
            int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
            if (ret != 0)
            {
                 // 4. Prepare JSON
                cJSON* root = cJSON_CreateObject();

                char key_buf[64];
                snprintf(key_buf, sizeof(key_buf), "FAN RPM GET");

                // 6. Fill in result (Success/fail)
                cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


                // 7. Print and release
                char* out = cJSON_Print(root);
                json_printf("%s\n", out);

                free(out);
                cJSON_Delete(root);
                return -1;
            }

            // Matching image 1: FAN RPM Get
            cJSON* root = cJSON_CreateObject();

            // Note: FANcount value is string "1"
            cJSON_AddStringToObject(root, "FANcount", "1");
            
            char key_buf[16];
            char val_buf[16];
            snprintf(key_buf, sizeof(key_buf), "FAN%d",  1);
            snprintf(val_buf, sizeof(val_buf), "%d", 1350000/boardInfo.fan_rpm);

            // Add to inner object
            cJSON_AddStringToObject(root, key_buf, val_buf);

            char* out = cJSON_Print(root);
            json_printf("%s\n", out);
            free(out);
            cJSON_Delete(root);
        }
        else {
            dbg_printf("[Error] FAN RPM only supports 'Get'\n");
            return -1;
        }
    }
    // =========================================================
    // 2. Process FAN Duty
    // =========================================================
    else if (STR_EQUAL(sub_type, "Duty")) {

        // --- Condition A: Duty Get ---
        if (STR_EQUAL(action, "Get")) {
            M463_BoardData_t boardInfo;
  
            int ret = usb_read_multi_bmc((unsigned char)count, &boardInfo);
            if (ret != 0)
            {
                 // 4. Prepare JSON
                cJSON* root = cJSON_CreateObject();

                char key_buf[64];
                snprintf(key_buf, sizeof(key_buf), "FAN DUTY GET");

                // 6. Fill in result (Success/fail)
                cJSON_AddStringToObject(root, key_buf, "fail, no supports port");


                // 7. Print and release
                char* out = cJSON_Print(root);
                json_printf("%s\n", out);

                free(out);
                cJSON_Delete(root);
                return -1;
            }


            // Matching image 2: FAN Duty Get
            cJSON* root = cJSON_CreateObject();

            cJSON_AddStringToObject(root, "FANcount", "1");

            char key_buf[16];
            char val_buf[16];
            snprintf(key_buf, sizeof(key_buf), "FAN%d",  1);
            snprintf(val_buf, sizeof(val_buf), "%d", (boardInfo.fan_duty * 100 /255));

            // Add to inner object
            cJSON_AddStringToObject(root, key_buf, val_buf);

            char* out = cJSON_Print(root);
            json_printf("%s\n", out);
            free(out);
            cJSON_Delete(root);
        }
        // --- Condition B: Duty Set ---
        else if (STR_EQUAL(action, "Set")) {
            // Matching image 2 three types of Set usage
            // argv[0]=FAN, [1]=Duty, [2]=Set, [3]=Arg1, [4]=Arg2...
            int is_success;
            if (argc < 4) {
                dbg_printf("[Error] Usage: FAN Duty Set <ID> <Duty> OR <AllDuty> OR <json_file>\n");
                return -1;
            }

            // Parameter logic determination
            // Case 1: Set <file.json>
            if (strstr(argv[3], ".json")) {
                dbg_printf("[Action] Load FAN config from JSON file: %s\n", argv[3]);
            }
            // Case 2: Set <ID> <Duty>
            else if (argc >= 5) {
                int fan_id = atoi(argv[3]);
                int duty_val = atoi(argv[4]);
                dbg_printf("[Action] Set FAN%d Duty to %d%%\n", fan_id, duty_val);
                if (duty_val > 100)
                    duty_val = 100;
                if (usbd_multi_pass_i2c_set((unsigned char)count, nct7363_adr, nct7363_duty_reg, DUTY_TO_REG8(duty_val)) == 0) {
                    is_success = 1;
                }
                else
                {
                    is_success = 0;
                }


            }
            // Case 3: Set <AllDuty>
            else {
                int all_duty = atoi(argv[3]);
                if (all_duty > 100)
                    all_duty = 100;
                dbg_printf("[Action] Set ALL FANs Duty to %d%%\n", all_duty);

                if (usbd_multi_pass_i2c_set((unsigned char)count, nct7363_adr, nct7363_duty_reg, DUTY_TO_REG8(all_duty)) == 0) {
                    is_success = 1;
                }
                else
                {
                    is_success = 0;
                }

            }
            if (is_success == 1)
            {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "FAN_Duty_Set", "Success");
                char* out = cJSON_PrintUnformatted(root);
                dbg_printf("%s\n", out);
                free(out);
                cJSON_Delete(root);
            }
            else {
            
                cJSON* root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "FAN_Duty_Set", "False");
                char* out = cJSON_PrintUnformatted(root);
                dbg_printf("%s\n", out);
                free(out);
                cJSON_Delete(root);
            }
        }
        else {
            dbg_printf("[Error] Unknown Action: %s\n", action);
            return -1;
        }
    }
    else {
        dbg_printf("[Error] Unknown FAN sub-command: %s (Use RPM or Duty)\n", sub_type);
        return -1;
    }

    return 0;
}
int handle_global(int count, int argc, char* argv[]) {
    // Context:
    // argv[0] = "Global" (passed from cmd_table)
    // argv[1] = "LED"
    // argv[2] = "Status"

    // 1. Check argument integrity
    if (argc < 3 || !STR_EQUAL(argv[1], "LED") || !STR_EQUAL(argv[2], "Status")) {
        dbg_printf("[Error] Usage: Global LED Status\n");
        return -1;
    }
    unsigned char pGLED_buf[2] = { 0 };
    if (usbd_multi_pass_GLOBAL_get((unsigned char)count, &pGLED_buf[0]) == 0)
    {
        cJSON* root = cJSON_CreateObject();
        if (pGLED_buf[0] == 0x0)
        {
            cJSON_AddStringToObject(root, "GLED_AMB_N_R", "0");
        }
        else
        {
            cJSON_AddStringToObject(root, "GLED_AMB_N_R", "1");
        }
        if (pGLED_buf[1] == 0x0)
        {
            cJSON_AddStringToObject(root, "GLED_GRN_N_R", "0");
        }
        else
        {
            cJSON_AddStringToObject(root, "GLED_GRN_N_R", "1");
        }


        char* out = cJSON_PrintUnformatted(root);
        dbg_printf("%s\n", out);
        free(out);
        cJSON_Delete(root);
    }
    else
    {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "Global LED Statu", "False");
        char* out = cJSON_PrintUnformatted(root);
        dbg_printf("%s\n", out);
        free(out);
        cJSON_Delete(root);
    }
    return 0;
}
// ---------------------------------------------------------
// 3. Main Look-up Table
// ---------------------------------------------------------
// Categorizing the 14 commands into these main categories
cmd_entry_t cmd_table[] = {
    {"CPLD",      handle_cpld,       "CPLD Id / Version / Get / Set"},
    {"HDD",       handle_hdd,        "HDD Info / Port / LED / Temp / NVMe-Mi"},
    {"Update",    handle_update,     "Update <M463/CPLD>"},
    {"Version",   handle_version,    "Version <MCU>"},
    {"BPB",       handle_bpb,        "BPB Sensor Get"},
    {"Reset",     handle_reset,      "Reset system"},
    {"I2CBYPASS", handle_i2c_bypass, "I2C Bypass <Cmd>"},
    
    // [NEW] Added FAN command
    {"FAN",       handle_fan,        "FAN RPM / Duty <Get/Set>"},
    {"Global",    handle_global,     "Global LED Status"},
    // [NEW] Added: register usblist command
    {"usblist",   handle_usblist,    "List USB devices"},


    {"appver",   handle_appversion,    "app verison information"},
    {"dumpall",   handle_dumpall,    "dump all information"},

    {NULL, NULL, NULL} // End marker
};

// ---------------------------------------------------------
// 4. Main Function
// ---------------------------------------------------------
int main(int argc, char* argv[]) {

    if (argc < 3) {
        dbg_printf("Usage: %s <Count> <Command> [Args...]\n", argv[0]);
        dbg_printf("Available Commands:\n");
        for (int i = 0; cmd_table[i].name != NULL; i++) {
            dbg_printf("  %-10s : %s\n", cmd_table[i].name, cmd_table[i].help);
        }
        return 1;
    }

    int count = atoi(argv[1]);

    char* target_cmd = argv[2];
    int found = 0;

    for (int i = 0; cmd_table[i].name != NULL; i++) {
        if (STR_EQUAL(target_cmd, cmd_table[i].name)) {
            cmd_table[i].handler(count, argc - 2, argv + 2);
            found = 1;
            break;
        }
    }

    if (!found) {
        dbg_printf("Error: Unknown command '%s'\n", target_cmd);
        return 1;
    }

    return 0;
}