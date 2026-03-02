//goble define 
#define USB_SERIAL_STR_LEN  50






// user pin define
#define GLED_AMB_N_R PB3
#define GLED_GRN_N_R PB2
#define HWM_SEL PA9

#define tempersensor_adr (0x30>>1)
#define cpld_adr (0xf0>>1)
#define TCA9548 (0xE0>>1)
#define nct7363_adr (0x46>>1)


#define cpld_ver 0x0
#define cpld_test_ver 0x2
#define cpld_jtag_id 0x20
#define cpld_hdd_amount 0x30
#define cpld_hdd_port_status 0x40
#define cpld_hdd_status 0x50
#define cpld_hdd_led 0x60
#define cpld_hdd_max_cnt 8
#define map_tempersensor_high 0x10
#define map_tempersensor_low 0x11
#define FAN_DATA_OFFSET         0x70 // Starting offset for fan data
#define FAN_DATA_BLOCK_SIZE     4    // 4 bytes per fan: duty, rpm_h, rpm_l, reserved
#define MAX_LOGICAL_FANS        5    // Should match LOGICAL_FAN_MAX_COUNT
// Offsets within each fan data block
#define FAN_DUTY_OFFSET         0
#define FAN_RPM_HIGH_OFFSET     1
#define FAN_RPM_LOW_OFFSET      2
#define REG_TEMP_DATA           0x05 // Temperature Data Register
//#define map_cpld_ver 0x00
//#define map_cpld_test_ver 0x02
//#define map_cpld_hdd_amount 0x30


//#define i2c_mux_cnt 6
#define NVME_TP1_ADDR 0xd4>>1
#define NVME_TP2_ADDR 0x56>>1
#define NVME_TP3_ADDR 0x90>>1
#define NVME_MEM_OFFSET 0x100
#define NVME_READ_REG 0x0
#define NVME_READ_COUNT 32


#define TEMP_RESOLUTION         0.0625f // 0.0625?XC per LSB

#define nvme_slot_0 0x100
#define nvme_slot_1 0x120
#define nvme_slot_2 0x140
#define nvme_slot_3 0x160
#define nvme_slot_4 0x180
#define nvme_slot_5 0x1a0
extern volatile uint8_t bmc_report[1024] __attribute__((aligned(4))) ;
extern volatile uint8_t g_u8DumpLogFlag;
extern void tempersensor_read(void);
extern void fan_read(void);
extern void CPLD_read(void);
extern void CPLD_read_AFTER(void);
extern void fan_inital(void);
extern void Set_USB_SerialNumber_From_UID(void);
extern void nvm_mi_read(void);
extern float show_temperature(uint8_t h_bytem, uint8_t l_byte);
extern void show_cpld_information(uint8_t *p_buf);
extern void print_nvme_basic_management_info(uint8_t *data);