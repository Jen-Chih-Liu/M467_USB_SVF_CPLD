#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
#include <stdbool.h>

void CPLD_read(void)
{
    unsigned char local_cnt; // Local counter for loops.

    // Read the CPLD version register.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_ver) == 0)
    {
        bmc_report[cpld_ver] = I2C_ReadByte(I2C0, cpld_adr);
			// printf("cv=0x%x\n\r", bmc_report[cpld_ver]);
    }else{
		   // printf("read cpld false\n\r");
		}

    // Read the CPLD test version register.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_test_ver) == 0)
    {
        bmc_report[cpld_test_ver] = I2C_ReadByte(I2C0, cpld_adr);
    }

    // Read the register that indicates the number of installed HDDs.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_amount) == 0)
    {
        bmc_report[cpld_hdd_amount] = I2C_ReadByte(I2C0, cpld_adr);
    }

    // Loop through all possible HDD slots to read their respective statuses.
    for (local_cnt = 0; local_cnt < cpld_hdd_max_cnt; local_cnt++)
    {
        // Read the CPLD HDD port status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_port_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_port_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        // Read the CPLD HDD general status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        // Read the CPLD HDD LED status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_led + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_led + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }
    }
}

/**
 * @brief Reads CPLD data and prints the version for debugging.
 * @details This function is a variant of CPLD_read(). It performs the same I2C read
 *          operations but includes active printf statements to output the CPLD
 *          version and error messages to the console, likely for debugging purposes.
 */
void CPLD_read_AFTER(void)
{
    unsigned char local_cnt;

    // Read the CPLD version register and print it.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_ver) == 0)
    {
        bmc_report[cpld_ver] = I2C_ReadByte(I2C0, cpld_adr);
			 printf("cv=0x%x\n\r", bmc_report[cpld_ver]);
    }else{
		    printf("read cpld false\n\r");
		}

    // Read the CPLD test version register.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_test_ver) == 0)
    {
        bmc_report[cpld_test_ver] = I2C_ReadByte(I2C0, cpld_adr);
    }
    // Read the register that indicates the number of installed HDDs.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_amount) == 0)
    {
        bmc_report[cpld_hdd_amount] = I2C_ReadByte(I2C0, cpld_adr);
    }
    // Loop through all possible HDD slots to read their respective statuses.
    for (local_cnt = 0; local_cnt < cpld_hdd_max_cnt; local_cnt++)
    {
        // Read the CPLD HDD port status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_port_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_port_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        // Read the CPLD HDD general status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        // Read the CPLD HDD LED status for the current slot.
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_led + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_led + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

    }
}
/**
 * @brief Reads the temperature from an external temperature sensor via I2C.
 * @details This function reads two bytes (high and low) from the temperature
 *          sensor's data register and stores them in the global `bmc_report` buffer.
 */
void tempersensor_read(void)
{
	unsigned char temp_buf[32];
    // Set the register pointer to the temperature data register.
    if (I2C_WriteByte(I2C0, tempersensor_adr, REG_TEMP_DATA) == 0)
    {
        // Read the 2-byte temperature value.
        I2C_ReadMultiBytes(I2C0, tempersensor_adr, temp_buf, 2);
        // Store the high and low bytes of the temperature reading.
        bmc_report[map_tempersensor_high] = temp_buf[0];
        bmc_report[map_tempersensor_low] = temp_buf[1];
    }
}


/* ==============================================================================
 * 1. Register Definitions (Based on NCT7363Y Datasheet)
 * ============================================================================== */
#define NCT7363Y_REG_PWM_EN_0_7       0x38
#define NCT7363Y_REG_PWM_EN_8_15      0x39
#define NCT7363Y_REG_FANIN_GLOBAL_EN  0x40  // Bit 7: START_FC
#define NCT7363Y_REG_FANIN_EN_0_7     0x41
#define NCT7363Y_REG_FANIN_EN_8_15    0x42

#define NCT7363Y_REG_PWM_DUTY_BASE    0x90  // PWM0 Duty, PWM1 is 0x92, and so on (+ ch*2)
#define NCT7363Y_REG_PWM_DIV_BASE     0x91  // PWM0 Divisor, PWM1 is 0x93 (+ ch*2)
#define NCT7363Y_REG_PWM_CFG_BASE     0xB0  // PWM0/1 CFG is 0xB0, PWM2/3 is 0xB1 (+ ch/2)

#define NCT7363Y_REG_FANIN_VAL_H      0x48  // FANIN0 High Byte (+ ch*2)
#define NCT7363Y_REG_FANIN_VAL_L      0x49  // FANIN0 Low Byte  (+ ch*2)

/* Define NCT7363Y I2C addresses */
#define NCT7363Y_ADDR_1               0x44
#define NCT7363Y_ADDR_2               0x46

/* ==============================================================================
 * 2. Logical Fan Definitions (Application Interface)
 * ============================================================================== */
typedef enum {
    LOGICAL_FAN_CPU_1 = 0,
    LOGICAL_FAN_CPU_2,
    LOGICAL_FAN_SYS_FRONT,
    LOGICAL_FAN_SYS_REAR,
    LOGICAL_FAN_HDD_ZONE,
    LOGICAL_FAN_MAX_COUNT
} LogicalFanId_t;

/* Machine model definitions */
typedef enum {
    MACHINE_MODEL_A_HIGH_END, // High-end model: all fans connected, regular configuration
    //MACHINE_MODEL_B_LOW_END,  // Low-end model: only some fans
   //MACHINE_MODEL_C_SCATTERED // Special model: PWM and TACH are completely scattered and non-contiguous
} MachineModel_t;

/* ==============================================================================
 * 3. Physical Mapping Table Structure (Core Design: Isolate Hardware Differences)
 * ============================================================================== */
typedef struct {
    bool     is_present; // Whether this fan is present in this model
    uint8_t  i2c_addr;   // 0x44 or 0x46
    uint8_t  pwm_ch;     // NCT7363Y PWM channel (0-15)
    uint8_t  tach_ch;    // NCT7363Y FANIN channel (0-15)
} FanHwMapping_t;

typedef struct {
    MachineModel_t model_id;
    const FanHwMapping_t* fan_map; // Pointer to the fan array for this model
    uint8_t max_logical_fans;
} FanConfig_t;

/* ==============================================================================
 * 4. Configuration Tables for Different Models
 * ============================================================================== */

/* Model A (High-end) Configuration: Uses two NCT7363Y, contiguous channels */
static const FanHwMapping_t ModelA_FanMap[LOGICAL_FAN_MAX_COUNT] = {
    // Logical ID            Present?  I2C Addr         PWM Ch,  TACH Ch
    [LOGICAL_FAN_CPU_1]     = {true, NCT7363Y_ADDR_1, 0,      0},
    [LOGICAL_FAN_CPU_2]     = {true, NCT7363Y_ADDR_1, 1,      1},
    [LOGICAL_FAN_SYS_FRONT] = {true, NCT7363Y_ADDR_1, 2,      2},
    [LOGICAL_FAN_SYS_REAR]  = {true, NCT7363Y_ADDR_2, 0,      0},
    [LOGICAL_FAN_HDD_ZONE]  = {true, NCT7363Y_ADDR_2, 1,      1},
};

/* Model B (Low-end) Configuration: Uses only one NCT7363Y, fewer fans */
static const FanHwMapping_t ModelB_FanMap[LOGICAL_FAN_MAX_COUNT] = {
    // Logical ID            Present?  I2C Addr         PWM Ch,  TACH Ch
    [LOGICAL_FAN_CPU_1]     = {true,  NCT7363Y_ADDR_1, 0,      0},
    [LOGICAL_FAN_CPU_2]     = {false, 0,               0,      0},
    [LOGICAL_FAN_SYS_FRONT] = {true,  NCT7363Y_ADDR_1, 2,      2},
    [LOGICAL_FAN_SYS_REAR]  = {false, 0,               0,      0},
    [LOGICAL_FAN_HDD_ZONE]  = {false, 0,               0,      0},
};

/* Model C (Extremely Scattered) Configuration: Shows cross-IC, non-contiguous, non-matching PWM/TACH channels */
static const FanHwMapping_t ModelC_Scattered_FanMap[LOGICAL_FAN_MAX_COUNT] = {
    // Logical ID            Present?  I2C Addr         PWM Ch,  TACH Ch
    [LOGICAL_FAN_CPU_1]     = {true, NCT7363Y_ADDR_1, 13,     2},   // Scattered: PWM on Ch13, Tach on Ch2
    [LOGICAL_FAN_CPU_2]     = {true, NCT7363Y_ADDR_2, 7,      15},  // Different IC, PWM on Ch7, Tach on Ch15
    [LOGICAL_FAN_SYS_FRONT] = {true, NCT7363Y_ADDR_1, 0,      8},   // PWM 0 paired with TACH 8
    [LOGICAL_FAN_SYS_REAR]  = {true, NCT7363Y_ADDR_2, 10,     0},   // PWM 10 paired with TACH 0
    [LOGICAL_FAN_HDD_ZONE]  = {false,0,               0,      0},   // Not present
};

/* System Configuration List */
static const FanConfig_t SystemConfigs[] = {
    {MACHINE_MODEL_A_HIGH_END,  ModelA_FanMap,           LOGICAL_FAN_MAX_COUNT},
   // {MACHINE_MODEL_B_LOW_END,   ModelB_FanMap,           LOGICAL_FAN_MAX_COUNT},
    //{MACHINE_MODEL_C_SCATTERED, ModelC_Scattered_FanMap, LOGICAL_FAN_MAX_COUNT},
};

/* Currently loaded system configuration */
static const FanConfig_t* CurrentConfig = NULL;

/* ==============================================================================
 * 5. Virtual I2C Low-Level Functions (Replace with your actual I2C API)
 * ============================================================================== */

// Helper function to write a single register to an I2C device
static int I2C_WriteReg(uint8_t i2c_addr, uint8_t reg, uint8_t data) {
    uint8_t temp_buf[2];
    temp_buf[0] = reg;
    temp_buf[1] = data;
    // The project's I2C functions use a 7-bit address.
    if (I2C_WriteMultiBytes(I2C0, i2c_addr >> 1, temp_buf, 2) == 2) {
        return 0; // Success
    }
    return -1; // Fail
}

// Helper function to read a single register from an I2C device
static int I2C_ReadReg(uint8_t i2c_addr, uint8_t reg, uint8_t* data) {
    // Set the register pointer first
    if (I2C_WriteByte(I2C0, i2c_addr >> 1, reg) != 0) {
        return -1; // Fail to write register address
    }
    // Then read the data
    *data = I2C_ReadByte(I2C0, i2c_addr >> 1);
    return 0; // Success
}

/* ==============================================================================
 * 6. NCT7363Y Chip Driver Layer (Independent of logical models, manages IC only)
 * ============================================================================== */

/**
 * @brief Sets the PWM duty for a single NCT7363Y channel.
 */
int NCT7363Y_SetPwmDuty(uint8_t i2c_addr, uint8_t pwm_ch, uint8_t duty) {
    if (pwm_ch > 15) return -1;
    uint8_t reg_addr = NCT7363Y_REG_PWM_DUTY_BASE + (pwm_ch * 2);
    return I2C_WriteReg(i2c_addr, reg_addr, duty);
}

/**
 * @brief Reads the RPM for a single NCT7363Y channel.
 */
int NCT7363Y_GetRpm(uint8_t i2c_addr, uint8_t tach_ch, uint32_t* rpm) {
    if (tach_ch > 15) return -1;
    uint8_t val_h = 0, val_l = 0;
    
    I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_H + (tach_ch * 2), &val_h);
    I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_L + (tach_ch * 2), &val_l);
    
    uint16_t count13 = (val_h << 5) | (val_l & 0x1F); // 13-bit value
    
    if (count13 > 0) {
        *rpm = 1350000 / count13; // Assuming Poles = 4
    } else {
        *rpm = 0;
    }
    return 0;
}

/* ==============================================================================
 * 7. System Fan Management Layer (Fan Manager)
 * ============================================================================== */

/**
 * @brief System Initialization: Detects model, loads mapping table, and initializes hardware.
 */
void FanManager_Init(MachineModel_t current_model) {
    int i;
    for (i = 0; i < sizeof(SystemConfigs)/sizeof(SystemConfigs[0]); i++) {
        if (SystemConfigs[i].model_id == current_model) {
            CurrentConfig = &SystemConfigs[i];
            printf("Fan Manager initialized for Model %d\n", current_model);
            break;
        }
    }
    
    if (CurrentConfig == NULL) {
        printf("Error: Model not found!\n");
        return;
    }

    for (i = 0; i < CurrentConfig->max_logical_fans; i++) {
        const FanHwMapping_t* hw_map = &CurrentConfig->fan_map[i];

        if (hw_map->is_present) {
            uint8_t i2c_addr = hw_map->i2c_addr;
            uint8_t pwm_ch   = hw_map->pwm_ch;
            uint8_t tach_ch  = hw_map->tach_ch;
            uint8_t reg_addr, val;

            reg_addr = (pwm_ch < 8) ? NCT7363Y_REG_PWM_EN_0_7 : NCT7363Y_REG_PWM_EN_8_15;
            I2C_ReadReg(i2c_addr, reg_addr, &val);
            val |= (1 << (pwm_ch % 8));
            I2C_WriteReg(i2c_addr, reg_addr, val);

            // 2. Set PWM resolution to 1/256 (Modify STEP bit in FSCPxCFG registers B0h~B7h)
            // NCT7363Y shares one CFG register for every two channels: B0h for PWM0/1, B1h for PWM2/3...
            reg_addr = NCT7363Y_REG_PWM_CFG_BASE + (pwm_ch / 2);
            I2C_ReadReg(i2c_addr, reg_addr, &val);
            if (pwm_ch % 2 == 0) {
                val |= (1 << 2); // Even channel: Set PWM(2x) STEP bit (Bit 2) = 1 (1/256 resolution)
            } else {
                val |= (1 << 6); // Odd channel: Set PWM(2x+1) STEP bit (Bit 6) = 1 (1/256 resolution)
            }
            I2C_WriteReg(i2c_addr, reg_addr, val);


            reg_addr = (tach_ch < 8) ? NCT7363Y_REG_FANIN_EN_0_7 : NCT7363Y_REG_FANIN_EN_8_15;
            I2C_ReadReg(i2c_addr, reg_addr, &val);
            val |= (1 << (tach_ch % 8));
            I2C_WriteReg(i2c_addr, reg_addr, val);

            I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_GLOBAL_EN, &val);
            if ((val & 0x80) == 0) { 
                val |= 0x80;
                I2C_WriteReg(i2c_addr, NCT7363Y_REG_FANIN_GLOBAL_EN, val);
            }
        }
    }
}

/**
 * @brief Application Interface: Sets logical fan speed (0-100%).
 */
int FanManager_SetFanSpeed(LogicalFanId_t fan_id, uint8_t duty_percent) {
    if (CurrentConfig == NULL || fan_id >= CurrentConfig->max_logical_fans) return -1;
    const FanHwMapping_t* hw_map = &CurrentConfig->fan_map[fan_id];
    if (!hw_map->is_present) return 0;
        
    // Protection mechanism: Ensure the value passed from the application layer does not exceed 100%.
    if (duty_percent > 100) {
        duty_percent = 100;
    }
    
    // Convert 0~100 percentage to 0~255 hardware PWM value (compatible with STEP=1 resolution).
    uint8_t hw_duty = (duty_percent * 255) / 100;
    return NCT7363Y_SetPwmDuty(hw_map->i2c_addr, hw_map->pwm_ch, hw_duty);
}

/**
 * @brief Application Interface: Reads logical fan speed (RPM).
 */
int FanManager_GetFanRpm(LogicalFanId_t fan_id, uint32_t* rpm) {
    if (CurrentConfig == NULL || fan_id >= CurrentConfig->max_logical_fans) return -1;
    const FanHwMapping_t* hw_map = &CurrentConfig->fan_map[fan_id];
    if (!hw_map->is_present) {
        *rpm = 0;
        return 0;
    }
    return NCT7363Y_GetRpm(hw_map->i2c_addr, hw_map->tach_ch, rpm);
}

void fan_read(void)
{
    if (CurrentConfig == NULL) return;

    uint8_t i;
    for (i = 0; i < CurrentConfig->max_logical_fans; i++)
    {
        const FanHwMapping_t* hw_map = &CurrentConfig->fan_map[i];
        uint8_t* pFanData = &bmc_report[FAN_DATA_OFFSET + (i * FAN_DATA_BLOCK_SIZE)];

        if (hw_map->is_present)
        {
            uint8_t val_h = 0, val_l = 0, duty = 0;
            uint8_t i2c_addr = hw_map->i2c_addr;
            uint8_t tach_ch = hw_map->tach_ch;
            uint8_t pwm_ch = hw_map->pwm_ch;

            I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_H + (tach_ch * 2), &val_h);
            I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_L + (tach_ch * 2), &val_l);
            I2C_ReadReg(i2c_addr, NCT7363Y_REG_PWM_DUTY_BASE + (pwm_ch * 2), &duty);

            pFanData[FAN_RPM_HIGH_OFFSET] = val_h;
            pFanData[FAN_RPM_LOW_OFFSET] = val_l;
            pFanData[FAN_DUTY_OFFSET] = duty;
        }
        else
        {
            memset(pFanData, 0xFF, FAN_DATA_BLOCK_SIZE);
        }
    }
}

void fan_inital(void)
{
    // For this implementation, we are hardcoding it to a default model.
    // You can change MACHINE_MODEL_A_HIGH_END to another model if needed,
    // or implement dynamic detection.
    FanManager_Init(MACHINE_MODEL_A_HIGH_END);

    // Example of setting a fan's speed after initialization
    FanManager_SetFanSpeed(LOGICAL_FAN_CPU_1, 30); // Set CPU Fan 1 to 30% duty
}