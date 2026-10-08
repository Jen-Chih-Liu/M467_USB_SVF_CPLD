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
#define NVME_TP0_ADDR (0xd4>>1)
#define NVME_TP1_ADDR (0x56>>1)
#define NVME_TP2_ADDR (0x90>>1)
#define NVME_TP3_ADDR (0x36>>1)
#define NVME_MEM_OFFSET 0x100
#define NVME_READ_REG 0x0
#define NVME_READ_COUNT 32

/* Define NCT7363Y I2C addresses */
#define NCT7363Y_ADDR_0               (0x40>>1)
#define NCT7363Y_ADDR_1               (0x42>>1)
#define NCT7363Y_ADDR_2               (0x44>>1)
#define NCT7363Y_ADDR_3               (0x46>>1)

#define TEMP_RESOLUTION         0.0625f // 0.0625?XC per LSB

#define nvme_slot_0 0x100
#define nvme_slot_1 0x120
#define nvme_slot_2 0x140
#define nvme_slot_3 0x160
#define nvme_slot_4 0x180
#define nvme_slot_5 0x1a0
#define nvme_slot_6 0x1c0
#define nvme_slot_7 0x1e0
#define nvme_slot_8 0x200
#define nvme_slot_9 0x220
#define nvme_slot_10 0x240
#define nvme_slot_11 0x260
#define nvme_slot_12 0x280
#define nvme_slot_13 0x2a0
#define nvme_slot_14 0x2c0
#define nvme_slot_15 0x2e0
#define nvme_slot_16 0x300
#define nvme_slot_17 0x320
#define nvme_slot_18 0x340
#define nvme_slot_19 0x360
#define nvme_slot_20 0x380
#define nvme_slot_21 0x3a0
#define nvme_slot_22 0x3c0
#define nvme_slot_23 0x3e0
extern volatile uint8_t bmc_report[1024] __attribute__((aligned(4))) ;
extern volatile uint8_t bmc_report1[1024] __attribute__((aligned(4))) ;
extern volatile uint8_t g_u8DumpLogFlag;
extern void tempersensor_read(void);
extern void fan_read(void);
extern int CPLD_read(void);
extern int CPLD_read1(void);
extern void CPLD_read_AFTER(void);
extern void fan_inital(void);
extern int FanIC_BackupRegisters(void);
extern void Set_USB_SerialNumber_From_UID(void);
extern void nvm_mi_read(void);
extern void nvm_mi_read_1(void);
/* Execute one native NVMe-MI command (Config Get / VPD Read) over MCTP/SMBus.
 * Returns 0 on transport success (check *pu8MiStatus for command status),
 * negative on transport error. See nvme_mi.c for the full contract. */
extern int32_t nvme_mi_native_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode,
                                  uint32_t u32Dword0, uint32_t u32Dword1,
                                  uint8_t *pu8Out, uint16_t u16MaxOut,
                                  uint16_t *pu16OutLen, uint8_t *pu8MiStatus);
/* Execute one NVMe Admin passthrough command over NVMe-MI (NMIMT=2h). Builds a
 * 64-byte SQE (opcode/NSID/DataLen/CDW10/CDW11), receives the multi-packet
 * response and copies a window [u16OutOffset, +u16MaxOut) of the returned data
 * (host paging for logs larger than one USB frame). Returns 0 on transport
 * success (check *pu8MiStatus), negative on transport error. See nvme_mi.c. */
extern int32_t nvme_mi_admin_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode, uint32_t u32Nsid,
                                 uint32_t u32Cdw10, uint32_t u32Cdw11, uint32_t u32DataLen,
                                 uint16_t u16OutOffset, uint8_t *pu8Out, uint16_t u16MaxOut,
                                 uint16_t *pu16OutLen, uint16_t *pu16TotalLen, uint8_t *pu8MiStatus);
/* Execute one MCTP Control command (message type 0x00, no MIC) over MCTP/SMBus.
 * Command Code 0x03 = Get Endpoint UUID, 0x05 = Get Message Type Support. The
 * response data (resp[4 .. end)) is copied to pu8Out and the Completion Code to
 * *pu8CompCode (0 = Success). Returns 0 on transport success, negative on
 * transport error. See nvme_mi.c. */
extern int32_t nvme_mi_mctp_ctrl_cmd(uint8_t u8SlotIndex, uint8_t u8CmdCode,
                                     uint8_t *pu8Out, uint16_t u16MaxOut,
                                     uint16_t *pu16OutLen, uint8_t *pu8CompCode);
/* Execute an NVMe Admin Get Features command (opcode 0x0A) over NVMe-MI. No
 * data payload; the feature value is returned in the CQE DWORD0 (*pu32Result).
 * CDW10 = FID | (SEL << 8). *pu8Sc receives the NVMe command Status Code.
 * Returns 0 on transport success (check *pu8MiStatus / *pu8Sc). See nvme_mi.c. */
extern int32_t nvme_mi_admin_get_feature(uint8_t u8SlotIndex, uint8_t u8Fid, uint8_t u8Sel,
                                         uint32_t *pu32Result, uint8_t *pu8MiStatus,
                                         uint8_t *pu8Sc);
/* Execute one native NVMe-MI Command Set write (e.g. VPD Write, Opcode 0x06)
 * that appends u16DataLen data bytes after the request DWORDs and before the
 * MIC. The response payload (resp[5 .. end-MIC)) is copied to pu8Out and the
 * NVMe-MI Status byte to *pu8MiStatus (0 = Success). Returns 0 on transport
 * success, negative on transport error. See nvme_mi.c. */
extern int32_t nvme_mi_native_write_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode,
                                        uint32_t u32Dword0, uint32_t u32Dword1,
                                        const uint8_t *pu8Data, uint16_t u16DataLen,
                                        uint8_t *pu8Out, uint16_t u16MaxOut,
                                        uint16_t *pu16OutLen, uint8_t *pu8MiStatus);
/* !!! DESTRUCTIVE: erases all user data. !!! Execute an NVMe Admin Format NVM
 * command (Admin opcode 0x80) over NVMe-MI. No data payload; the CQE Status
 * Code is returned in *pu8Sc (0 = Success) and the NVMe-MI Status byte in
 * *pu8MiStatus. The receive is retried because Format can be slow. Returns 0 on
 * transport success, negative on transport error. See nvme_mi.c. */
extern int32_t nvme_mi_admin_format_nvm(uint8_t u8SlotIndex, uint32_t u32Nsid,
                                        uint32_t u32Cdw10, uint8_t *pu8MiStatus,
                                        uint8_t *pu8Sc);
extern float show_temperature(uint8_t h_bytem, uint8_t l_byte);
extern void show_cpld_information(uint8_t *p_buf);
extern void print_nvme_basic_management_info(uint8_t *data);
extern int FanIC_Backup_init(void);
extern int FanIC_CompareAndRestore(void);
extern void EEPROM_WriteData(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len);
extern void EEPROM_WriteData_1(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len);
extern void EEPROM_ReadData(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len);
extern void EEPROM_ReadData_1(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len);