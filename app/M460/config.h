#define dbg_printf printf
#define json_printf printf
#define cmd_printf printf
#define dumpall_printf printf
#define TARGET_VID  0x0416
#define TARGET_PID 0x502a



#define MAX_DEVICES 16  // Default maximum support for 16 identical devices
#define app_version_number "2025121901"

#ifdef _WIN32
#define transfer_size 1025 // Windows USB receiver size 
#else
#define transfer_size 1024 // Linux USB receiver size
#endif 

#define INTERFACE_NUMBER 0

#define PACKET_SIZE 1024 // Send packet size

#define map_cpld_ver 0x00
#define map_cpld_test_ver 0x02
#define map_cpld_hdd_amount 0x30
#define map_tempersensor_high 0x10
#define map_tempersensor_low 0x11
#define cpld_chip_jtag_id 0x20
#define cpld_hdd_port_status 0x40
#define cpld_hdd_status 0x50
#define cpld_hdd_led 0x60
#define cpld_hdd_max_cnt 8
#define cpld_jtag_id  
#define TEMP_RESOLUTION         0.0625f
#define nvme_slot_0_offset 0x100
#define nvme_slot_1_offset 0x120
#define nvme_slot_2_offset 0x140
#define nvme_slot_3_offset 0x160
#define nvme_slot_4_offset 0x180
#define nvme_slot_5_offset 0x1a0
#define map_fan_duty 0x70
#define map_fan_rpm_high 0x80
#define map_fan_rpm_low 0x81
#define NVME_INFO_LEN 32 

#define cpld_adr (0xf0)
#define nct7363_adr (0x46)
#define nct7363_duty_reg (0x90)
typedef struct {
    // --- Board Information (Offset 0x0 - 0x4) ---

    uint8_t cpld_version;     // 0x0
    uint8_t cpld_test_ver;    // 0x1
    uint8_t temp_sensor_high; // 0x10
    uint8_t temp_sensor_low;  // 0x11
    uint8_t hdd_amount;       // 0x30
    uint8_t fan_duty;
    uint16_t fan_rpm;
    uint32_t jtag_id;      // 0x20       
    // --- CPLD Register Maps (Note the gaps here) ---
    uint8_t cpld_reg_40_4f[16]; // Offset 0x40 - 0x4F (Image shows up to 47, but allocated to 4F)
    uint8_t cpld_reg_50_5f[16]; // Offset 0x50 - 0x5F
    uint8_t cpld_reg_60_6f[16]; // Offset 0x60 - 0x6F

    // --- NVMe Info Slots (Starts from Offset 0x100) ---
    uint8_t nvme_slot_0[NVME_INFO_LEN]; // 0x100 - 0x11F
    uint8_t nvme_slot_1[NVME_INFO_LEN]; // 0x120 - 0x13F
    uint8_t nvme_slot_2[NVME_INFO_LEN]; // 0x140 - 0x15F
    uint8_t nvme_slot_3[NVME_INFO_LEN]; // 0x160 - 0x17F
    uint8_t nvme_slot_4[NVME_INFO_LEN]; // 0x180 - 0x19F
    uint8_t nvme_slot_5[NVME_INFO_LEN]; // 0x1A0 - 0x1BF
    uint8_t nvme_slot_6[NVME_INFO_LEN]; // 0x1C0 - 0x1DF
    uint8_t nvme_slot_7[NVME_INFO_LEN]; // 0x1E0 - 0x1FF
    uint8_t nvme_slot_8[NVME_INFO_LEN]; // 0x200 - 0x21F
    uint8_t nvme_slot_9[NVME_INFO_LEN]; // 0x220 - 0x23F
    uint8_t nvme_slot_10[NVME_INFO_LEN]; // 0x240 - 0x25F
    uint8_t nvme_slot_11[NVME_INFO_LEN]; // 0x260 - 0x27F
    uint8_t nvme_slot_12[NVME_INFO_LEN]; // 0x280 - 0x29F
    uint8_t nvme_slot_13[NVME_INFO_LEN]; // 0x2A0 - 0x2BF
    uint8_t nvme_slot_14[NVME_INFO_LEN]; // 0x2C0 - 0x2DF
    uint8_t nvme_slot_15[NVME_INFO_LEN]; // 0x2E0 - 0x2FF

} M463_BoardData_t;


extern int usb_read_multi_bmc(unsigned char usb_cnt, M463_BoardData_t* pBoardData);
extern float show_temperature(uint8_t h_bytem, uint8_t l_byte);
extern int usb_read_multi_mcu_version(unsigned char usb_cnt, unsigned int* pMCUVERSIONData);
extern int usb_multi_mcu_reset(unsigned char usb_cnt);
extern int usbd_multi_cpld_i2c_get(unsigned char usb_cnt, unsigned char i2c_reg, unsigned char i2c_len, unsigned char* i2c_array);
extern int usbd_multi_cpld_i2c_set(unsigned char usb_cnt, unsigned char i2c_reg, unsigned char i2c_data);
extern int usbd_multi_pass_i2c_get(unsigned char usb_cnt, unsigned char i2c_addr, unsigned char i2c_reg, unsigned char i2c_len, unsigned char* i2c_array);
extern int usbd_multi_pass_i2c_set(unsigned char usb_cnt, unsigned char i2c_addr, unsigned char i2c_reg, unsigned char i2c_data);
extern int usbd_multi_mcu_jumper_ldrom(unsigned char usb_cnt);
extern int usb_multi_mcu_reset_set_var(unsigned char usb_cnt, unsigned char reset_var);
extern int usb_multi_mcu_reset_get_var(unsigned char usb_cnt, unsigned char* pReset_var);
extern int usbd_multi_pass_GLOBAL_get(unsigned char usb_cnt, unsigned char* LED_array);
extern int is_bin_file(const char* filename);
// extern int is_svf_file(const char* filename);
extern int isp_mcu(char* filename);
extern void scan_and_update_map(libusb_context* ctx, libusb_device** devs, ssize_t cnt);
extern void clear_map(void);
extern int open_specific_device_and_endpoints(libusb_device* dev, libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in);
extern const char* cpld_error_to_string(int err_code);
extern int cpld_svf_update(unsigned char usb_cnt, char* svf_file, char* fail_reason_buf, size_t buf_len);
extern int usbd_multi_mcu_stop_bmc_monitor(unsigned char usb_cnt);
extern int usbd_multi_mcu_start_bmc_monitor(unsigned char usb_cnt);
extern void print_nvme_basic_management_info(uint8_t* data);
extern int usbd_multi_mcu_eeprom_write(unsigned char usb_cnt, char* filename );
extern int usbd_multi_MCU_GPIO_SET(unsigned char usb_cnt, unsigned char gpio_num, unsigned char gpio_val);
extern int usb_multi_gpio_get_var(unsigned char usb_cnt, unsigned char* pgpio_val);

extern void sleep_seconds(double seconds);
typedef struct {
    libusb_device* device; // Stores device pointer for direct opening
    uint8_t bus;
    uint8_t port_path[8];
    int port_len;
} DeviceEntry;
extern DeviceEntry MyDeviceMap[MAX_DEVICES];
extern int g_device_count;
typedef enum {
    RES_FALSE = 0,
    RES_PASS,
    RES_FILE_NO_FOUND,
    RES_PROGRAM_FALSE,
    RES_CONNECT,
    RES_CONNECT_FALSE,
    RES_CHIP_FOUND,
    RES_DISCONNECT,
    RES_FILE_LOAD,
    RES_FILE_SIZE_OVER,
    RES_TIME_OUT,
    RES_SN_OK,
    RES_DETECT_MCU,
    RES_NO_DETECT,
    RES_FILE_SIZE_SMALL,
} ISP_STATE;
// CPLD SVF Error Codes
typedef enum {
    CPLD_SUCCESS = 0,           // Success
    CPLD_ERR_FILE_ACCESS,       // Cannot open or read SVF file
    CPLD_ERR_SVF_PARSE,         // SVF format parsing error (or memory allocation failure)
    CPLD_ERR_USB_INIT,          // Libusb initialization failed
    CPLD_ERR_NO_DEVICE,         // Target USB device not found
    CPLD_ERR_OPEN_DEVICE,       // Cannot open USB device (privilege or occupancy issue)
    CPLD_ERR_READ_VERSION,      // Cannot read FW version
    CPLD_ERR_JTAG_INIT,         // SVF/JTAG state initialization failed (0xA0 command)
    CPLD_ERR_PROGRAMMING,       // Error during programming process (Check Status != 0)
    CPLD_ERR_TIMEOUT,           // USB transfer timeout
    CPLD_ERR_UNKNOWN            // Unknown error
} CPLD_ERROR;

// LDROM macro definitions
#define aprom_size_t 256*1024

#define full_Pkg_Size 64
#define USB_VID_LD  0x0416
#define USB_PID_LD 0x3f00
