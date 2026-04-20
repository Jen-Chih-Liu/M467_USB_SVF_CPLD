#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
#include <stdbool.h>

int CPLD_read(void)
{
    unsigned char local_cnt; // Local counter for loops.

    // Read the CPLD version register.
    if (I2C_WriteByte(I2C0, cpld_adr, cpld_ver) == 0)
    {
        bmc_report[cpld_ver] = I2C_ReadByte(I2C0, cpld_adr);
        // printf("cv=0x%x\n\r", bmc_report[cpld_ver]);
    }
    else
    {
        return 0; //false
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

    return 1;
}


int  CPLD_read1(void)
{
    unsigned char local_cnt; // Local counter for loops.

    // Read the CPLD version register.
    if (I2C_WriteByte(I2C2, cpld_adr, cpld_ver) == 0)
    {
        bmc_report1[cpld_ver] = I2C_ReadByte(I2C2, cpld_adr);
        // printf("cv=0x%x\n\r", bmc_report[cpld_ver]);
    }
    else
    {
        return 0;
    }

    // Read the CPLD test version register.
    if (I2C_WriteByte(I2C2, cpld_adr, cpld_test_ver) == 0)
    {
        bmc_report1[cpld_test_ver] = I2C_ReadByte(I2C2, cpld_adr);
    }

    // Read the register that indicates the number of installed HDDs.
    if (I2C_WriteByte(I2C2, cpld_adr, cpld_hdd_amount) == 0)
    {
        bmc_report1[cpld_hdd_amount] = I2C_ReadByte(I2C2, cpld_adr);
    }

    // Loop through all possible HDD slots to read their respective statuses.
    for (local_cnt = 0; local_cnt < cpld_hdd_max_cnt; local_cnt++)
    {
        // Read the CPLD HDD port status for the current slot.
        if (I2C_WriteByte(I2C2, cpld_adr, cpld_hdd_port_status + local_cnt) == 0)
        {
            bmc_report1[cpld_hdd_port_status + local_cnt] = I2C_ReadByte(I2C2, cpld_adr);
        }

        // Read the CPLD HDD general status for the current slot.
        if (I2C_WriteByte(I2C2, cpld_adr, cpld_hdd_status + local_cnt) == 0)
        {
            bmc_report1[cpld_hdd_status + local_cnt] = I2C_ReadByte(I2C2, cpld_adr);
        }

        // Read the CPLD HDD LED status for the current slot.
        if (I2C_WriteByte(I2C2, cpld_adr, cpld_hdd_led + local_cnt) == 0)
        {
            bmc_report1[cpld_hdd_led + local_cnt] = I2C_ReadByte(I2C2, cpld_adr);
        }
    }

    return 1;
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
    }
    else
    {
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

void tempersensor_read1(void)
{
    unsigned char temp_buf[32];

    // Set the register pointer to the temperature data register.
    if (I2C_WriteByte(I2C2, tempersensor_adr, REG_TEMP_DATA) == 0)
    {
        // Read the 2-byte temperature value.
        I2C_ReadMultiBytes(I2C2, tempersensor_adr, temp_buf, 2);
        // Store the high and low bytes of the temperature reading.
        bmc_report1[map_tempersensor_high] = temp_buf[0];
        bmc_report1[map_tempersensor_low] = temp_buf[1];
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
typedef enum
{
    LOGICAL_FAN_1 = 0,
    LOGICAL_FAN_2,
    LOGICAL_FAN_3,
    LOGICAL_FAN_4,
    LOGICAL_FAN_5,
    LOGICAL_FAN_6,
    LOGICAL_FAN_7,
    LOGICAL_FAN_8,
    LOGICAL_FAN_9,
    LOGICAL_FAN_10,
    LOGICAL_FAN_11,
    LOGICAL_FAN_12,
    LOGICAL_FAN_13,
    LOGICAL_FAN_14,
    LOGICAL_FAN_15,
    LOGICAL_FAN_16,


    LOGICAL_FAN_MAX_COUNT
} LogicalFanId_t;

/* Machine model definitions */
typedef enum
{
    MACHINE_MODEL_A_HIGH_END, // High-end model: all fans connected, regular configuration
    //MACHINE_MODEL_B_LOW_END,  // Low-end model: only some fans
    //MACHINE_MODEL_C_SCATTERED // Special model: PWM and TACH are completely scattered and non-contiguous
} MachineModel_t;

/* ==============================================================================
 * 3. Physical Mapping Table Structure (Core Design: Isolate Hardware Differences)
 * ============================================================================== */
typedef struct
{
    bool     is_present; // Whether this fan is present in this model
    uint8_t  i2c_addr;   // 0x44 or 0x46
    uint8_t  pwm_ch;     // NCT7363Y PWM channel (0-15)
    uint8_t  tach_ch;    // NCT7363Y FANIN channel (0-15)
} FanHwMapping_t;

typedef struct
{
    MachineModel_t model_id;
    const FanHwMapping_t *fan_map; // Pointer to the fan array for this model
    uint8_t max_logical_fans;
} FanConfig_t;

/* ==============================================================================
 * 4. Configuration Tables for Different Models
 * ============================================================================== */

/* Model A (High-end) Configuration: Uses two NCT7363Y, contiguous channels */
static const FanHwMapping_t ModelA_FanMap[LOGICAL_FAN_MAX_COUNT] =
{
    // Logical ID            Present?  I2C Addr         PWM Ch,  TACH Ch
    [LOGICAL_FAN_1]     = {true, NCT7363Y_ADDR_1, 0,      0},
    [LOGICAL_FAN_2]     = {true, NCT7363Y_ADDR_1, 1,      1},
    [LOGICAL_FAN_3]     = {true, NCT7363Y_ADDR_1, 2,      2},
    [LOGICAL_FAN_4]     = {true, NCT7363Y_ADDR_1, 3,      3},
    [LOGICAL_FAN_5]     = {true, NCT7363Y_ADDR_1, 0,      4},
    [LOGICAL_FAN_6]     = {true, NCT7363Y_ADDR_1, 1,      5},
    [LOGICAL_FAN_7]     = {true, NCT7363Y_ADDR_1, 2,      6},
    [LOGICAL_FAN_8]     = {true, NCT7363Y_ADDR_1, 3,      7},
    [LOGICAL_FAN_9]     = {true, NCT7363Y_ADDR_2, 0,      0},
    [LOGICAL_FAN_10]    = {true, NCT7363Y_ADDR_2, 1,      1},
    [LOGICAL_FAN_11]    = {true, NCT7363Y_ADDR_2, 2,      2},
    [LOGICAL_FAN_12]    = {true, NCT7363Y_ADDR_2, 3,      3},
    [LOGICAL_FAN_13]    = {true, NCT7363Y_ADDR_2, 0,      4},
    [LOGICAL_FAN_14]    = {true, NCT7363Y_ADDR_2, 1,      5},
    [LOGICAL_FAN_15]    = {true, NCT7363Y_ADDR_2, 2,      6},
    [LOGICAL_FAN_16]    = {true, NCT7363Y_ADDR_2, 3,      7},
};


/* System Configuration List */
static const FanConfig_t SystemConfigs[] =
{
    {MACHINE_MODEL_A_HIGH_END,  ModelA_FanMap,           LOGICAL_FAN_MAX_COUNT},
    // {MACHINE_MODEL_B_LOW_END,   ModelB_FanMap,           LOGICAL_FAN_MAX_COUNT},
    //{MACHINE_MODEL_C_SCATTERED, ModelC_Scattered_FanMap, LOGICAL_FAN_MAX_COUNT},
};

/* Currently loaded system configuration */
static const FanConfig_t *CurrentConfig = NULL;

/* ==============================================================================
 * 5. Virtual I2C Low-Level Functions (Replace with your actual I2C API)
 * ============================================================================== */

// Helper function to write a single register to an I2C device
static int I2C_WriteReg(uint8_t i2c_addr, uint8_t reg, uint8_t data)
{
    uint8_t temp_buf[2];
    temp_buf[0] = reg;
    temp_buf[1] = data;

    // The project's I2C functions use a 7-bit address.
    if (I2C_WriteMultiBytes(I2C0, i2c_addr >> 1, temp_buf, 2) == 2)
    {
        return 0; // Success
    }

    return -1; // Fail
}

// Helper function to read a single register from an I2C device
static int I2C_ReadReg(uint8_t i2c_addr, uint8_t reg, uint8_t *data)
{
    // Set the register pointer first
    if (I2C_WriteByte(I2C0, i2c_addr >> 1, reg) != 0)
    {
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
int NCT7363Y_SetPwmDuty(uint8_t i2c_addr, uint8_t pwm_ch, uint8_t duty)
{
    if (pwm_ch > 15) return -1;

    uint8_t reg_addr = NCT7363Y_REG_PWM_DUTY_BASE + (pwm_ch * 2);
    return I2C_WriteReg(i2c_addr, reg_addr, duty);
}

/**
 * @brief Reads the RPM for a single NCT7363Y channel.
 */
int NCT7363Y_GetRpm(uint8_t i2c_addr, uint8_t tach_ch, uint32_t *rpm)
{
    if (tach_ch > 15) return -1;

    uint8_t val_h = 0, val_l = 0;

    I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_H + (tach_ch * 2), &val_h);
    I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_VAL_L + (tach_ch * 2), &val_l);

    uint16_t count13 = (val_h << 5) | (val_l & 0x1F); // 13-bit value

    if (count13 > 0)
    {
        *rpm = 1350000 / count13; // Assuming Poles = 4
    }
    else
    {
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
void FanManager_Init(MachineModel_t current_model)
{
    int i;

    for (i = 0; i < sizeof(SystemConfigs) / sizeof(SystemConfigs[0]); i++)
    {
        if (SystemConfigs[i].model_id == current_model)
        {
            CurrentConfig = &SystemConfigs[i];
            printf("Fan Manager initialized for Model %d\n", current_model);
            break;
        }
    }

    if (CurrentConfig == NULL)
    {
        printf("Error: Model not found!\n");
        return;
    }

    for (i = 0; i < CurrentConfig->max_logical_fans; i++)
    {
        const FanHwMapping_t *hw_map = &CurrentConfig->fan_map[i];

        if (hw_map->is_present)
        {
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

            if (pwm_ch % 2 == 0)
            {
                val |= (1 << 2); // Even channel: Set PWM(2x) STEP bit (Bit 2) = 1 (1/256 resolution)
            }
            else
            {
                val |= (1 << 6); // Odd channel: Set PWM(2x+1) STEP bit (Bit 6) = 1 (1/256 resolution)
            }

            I2C_WriteReg(i2c_addr, reg_addr, val);


            reg_addr = (tach_ch < 8) ? NCT7363Y_REG_FANIN_EN_0_7 : NCT7363Y_REG_FANIN_EN_8_15;
            I2C_ReadReg(i2c_addr, reg_addr, &val);
            val |= (1 << (tach_ch % 8));
            I2C_WriteReg(i2c_addr, reg_addr, val);

            I2C_ReadReg(i2c_addr, NCT7363Y_REG_FANIN_GLOBAL_EN, &val);

            if ((val & 0x80) == 0)
            {
                val |= 0x80;
                I2C_WriteReg(i2c_addr, NCT7363Y_REG_FANIN_GLOBAL_EN, val);
            }
        }
    }
}

/**
 * @brief Application Interface: Sets logical fan speed (0-100%).
 */
int FanManager_SetFanSpeed(LogicalFanId_t fan_id, uint8_t duty_percent)
{
    if (CurrentConfig == NULL || fan_id >= CurrentConfig->max_logical_fans) return -1;

    const FanHwMapping_t *hw_map = &CurrentConfig->fan_map[fan_id];

    if (!hw_map->is_present) return 0;

    // Protection mechanism: Ensure the value passed from the application layer does not exceed 100%.
    if (duty_percent > 100)
    {
        duty_percent = 100;
    }

    // Convert 0~100 percentage to 0~255 hardware PWM value (compatible with STEP=1 resolution).
    uint8_t hw_duty = (duty_percent * 255) / 100;
    return NCT7363Y_SetPwmDuty(hw_map->i2c_addr, hw_map->pwm_ch, hw_duty);
}

/**
 * @brief Application Interface: Reads logical fan speed (RPM).
 */
int FanManager_GetFanRpm(LogicalFanId_t fan_id, uint32_t *rpm)
{
    if (CurrentConfig == NULL || fan_id >= CurrentConfig->max_logical_fans) return -1;

    const FanHwMapping_t *hw_map = &CurrentConfig->fan_map[fan_id];

    if (!hw_map->is_present)
    {
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
        const FanHwMapping_t *hw_map = &CurrentConfig->fan_map[i];
        uint8_t *pFanData = &bmc_report[FAN_DATA_OFFSET + (i * FAN_DATA_BLOCK_SIZE)];

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
    FanManager_SetFanSpeed(LOGICAL_FAN_1, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_2, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_3, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_4, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_5, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_6, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_7, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_8, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_9, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_10, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_11, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_12, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_13, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_14, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_15, 30); // Set CPU Fan 1 to 30% duty
    FanManager_SetFanSpeed(LOGICAL_FAN_16, 30); // Set CPU Fan 1 to 30% duty
}

/**
 * @brief Backup all registers from two NCT7363Y fan ICs via I2C0.
 * @details This function reads all registers (0x00 to 0xFE) from both fan ICs
 *          at addresses 0x44 and 0x46, and stores them in the backup arrays.
 * @param fan_address_0x44 Pointer to backup array for IC at address 0x44
 * @param fan_address_0x46 Pointer to backup array for IC at address 0x46
 * @return 0 on success, -1 on failure
 */

volatile unsigned char fan_address_0x44_back[0xff];
volatile unsigned char fan_address_0x46_back[0xff];

extern volatile unsigned char fan_address_0x44[0xff];
extern volatile unsigned char fan_address_0x46[0xff];
int FanIC_BackupRegisters(void)
{
    uint16_t reg_index;
    uint8_t data_0x44, data_0x46;
    int result = 0;

    // Read all registers from both fan ICs
    for (reg_index = 0; reg_index < 0xFF; reg_index++)
    {
        // Read register from IC at address 0x44
        if (I2C_ReadReg(NCT7363Y_ADDR_1, (uint8_t)reg_index, &data_0x44) == 0)
        {
            fan_address_0x44_back[reg_index] = data_0x44;
        }


        // Read register from IC at address 0x46
        if (I2C_ReadReg(NCT7363Y_ADDR_2, (uint8_t)reg_index, &data_0x46) == 0)
        {
            fan_address_0x46_back[reg_index] = data_0x46;
        }
    }

    return result;
}

int FanIC_Backup_init(void)
{
    uint16_t reg_index;
    FanIC_BackupRegisters();

    // Read all registers from both fan ICs
    for (reg_index = 0; reg_index < 0xFF; reg_index++)
    {
        fan_address_0x44[reg_index] = fan_address_0x44_back[reg_index];
        fan_address_0x46[reg_index] = fan_address_0x46_back[reg_index];
    }
}

/**
 * @brief Check if a register is read-only (RO) and should not be written.
 * @param reg Register address to check
 * @return true if register is read-only, false if writable
 */
static bool Is_Register_ReadOnly(uint8_t reg)
{
    // GPIO0 Input Port Register
    if (reg == 0x00) return true;

    // GPIO0 Interrupt Status Register (R/clr)
    if (reg == 0x07) return true;

    // GPIO0 Input Latch Data Register
    if (reg == 0x08) return true;

    // GPIO1 Input Port Register
    if (reg == 0x10) return true;

    // GPIO1 Interrupt Status Register (R/clr)
    if (reg == 0x17) return true;

    // GPIO1 Input Latch Data Register
    if (reg == 0x18) return true;

    // FAN Low Speed Interrupt Status Register (R/clr)
    if (reg == 0x32 || reg == 0x33) return true;

    // FAN Low Speed Real Status Register
    if (reg == 0x34 || reg == 0x35) return true;

    // FANIN Count Value Registers (High and Low bytes for all channels)
    // 48h, 49h (FANIN0); 4Ah, 4Bh (FANIN1); ... 66h, 67h (FANIN15)
    if ((reg >= 0x48 && reg <= 0x67) && ((reg & 0x01) == 0 || (reg & 0x01) == 1))
    {
        // This covers 48h-67h range (FANIN0-FANIN15 count values)
        return true;
    }

    return false;
}

/**
 * @brief Compare and restore fan IC registers if modified.
 * @details This function compares the current fan IC register values with the backup.
 *          If any register has been modified and is writable, it writes the new value to the IC.
 *          Read-only registers are skipped.
 * @return Number of registers restored, -1 on error
 */
int FanIC_CompareAndRestore(void)
{
    uint16_t reg_index;
    int restore_count = 0;

    // Compare and restore registers for IC at address 0x44
    for (reg_index = 0; reg_index < 0xFF; reg_index++)
    {
        // Skip read-only registers
        if (Is_Register_ReadOnly((uint8_t)reg_index))
        {
            continue;
        }

        // Check if current value differs from backup
        if (fan_address_0x44[reg_index] != fan_address_0x44_back[reg_index])
        {
            // Write new value to I2C register
            if (I2C_WriteReg(NCT7363Y_ADDR_1, (uint8_t)reg_index, fan_address_0x44[reg_index]) == 0)
            {
                // Update backup with the new value
                fan_address_0x44_back[reg_index] = fan_address_0x44[reg_index];
                restore_count++;
            }
        }
    }

    // Compare and restore registers for IC at address 0x46
    for (reg_index = 0; reg_index < 0xFF; reg_index++)
    {
        // Skip read-only registers
        if (Is_Register_ReadOnly((uint8_t)reg_index))
        {
            continue;
        }

        // Check if current value differs from backup
        if (fan_address_0x46[reg_index] != fan_address_0x46_back[reg_index])
        {
            // Write new value to I2C register
            if (I2C_WriteReg(NCT7363Y_ADDR_2, (uint8_t)reg_index, fan_address_0x46[reg_index]) == 0)
            {
                // Update backup with the new value
                fan_address_0x46_back[reg_index] = fan_address_0x46[reg_index];
                restore_count++;
            }
        }
    }

    return restore_count;
}


#define EEPROM_SLAVE_ADDR  0xae>>1


/**
 * @brief   ACK Polling to check if EEPROM write cycle is complete (Not in Busy state)
 * @details After writing data, the EEPROM enters an internal write cycle (Max 5ms).
 * During this time, it will respond with NACK if addressed.
 * This function continuously sends SLA+W until the EEPROM responds with ACK.
 */
void EEPROM_WaitReady(void)
{
    uint32_t u32TimeOutCount = 0u;

    while (1)
    {
        I2C_START(I2C0);
        u32TimeOutCount = I2C_TIMEOUT / 2;
        I2C_WAIT_READY(I2C0)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }
        I2C_SET_DATA(I2C0, (EEPROM_SLAVE_ADDR << 1) | 0x00);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
        u32TimeOutCount = I2C_TIMEOUT / 2;
        I2C_WAIT_READY(I2C0)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }

        if (I2C_GET_STATUS(I2C0) == 0x18)
        {
            /* Received ACK (0x18), EEPROM has completed the write cycle and is ready */
            I2C_STOP(I2C0);
            u32TimeOutCount = I2C_TIMEOUT / 2;

            while ((I2C0)->CTL0 & I2C_CTL0_STO_Msk)
            {
                if (--u32TimeOutCount == 0)
                {

                    break;
                }
            }


            break;
        }

        /* Received NACK (0x20), send STOP and delay slightly before retrying */
        I2C_STOP(I2C0);
        u32TimeOutCount = I2C_TIMEOUT / 2;

        while ((I2C0)->CTL0 & I2C_CTL0_STO_Msk)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }
    }
}


void EEPROM_WaitReady_1(void)
{
    uint32_t u32TimeOutCount = 0u;

    while (1)
    {
        I2C_START(I2C2);
        u32TimeOutCount = I2C_TIMEOUT / 2;
        I2C_WAIT_READY(I2C2)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }
        I2C_SET_DATA(I2C2, (EEPROM_SLAVE_ADDR << 1) | 0x00);
        I2C_SET_CONTROL_REG(I2C2, I2C_CTL_SI);
        u32TimeOutCount = I2C_TIMEOUT / 2;
        I2C_WAIT_READY(I2C2)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }

        if (I2C_GET_STATUS(I2C2) == 0x18)
        {
            /* Received ACK (0x18), EEPROM has completed the write cycle and is ready */
            I2C_STOP(I2C2);
            u32TimeOutCount = I2C_TIMEOUT / 2;

            while ((I2C2)->CTL0 & I2C_CTL0_STO_Msk)
            {
                if (--u32TimeOutCount == 0)
                {

                    break;
                }
            }


            break;
        }

        /* Received NACK (0x20), send STOP and delay slightly before retrying */
        I2C_STOP(I2C2);
        u32TimeOutCount = I2C_TIMEOUT / 2;

        while ((I2C2)->CTL0 & I2C_CTL0_STO_Msk)
        {
            if (--u32TimeOutCount == 0)
            {

                break;
            }
        }
    }
}

/**
 * @brief   Low-level function: Single page continuous write (Cannot cross 16-Byte page boundary)
 * @details Uses BSP's I2C_WriteMultiBytes. Need to pack the address and data together manually.
 */
void EEPROM_WritePage(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    /* Create transmit buffer: 1 Byte memory address + up to 16 Bytes of data */
    uint8_t u8TxBuf[17];
    uint32_t i;

    /* Place the internal memory address at index 0 */
    u8TxBuf[0] = u8DataAddr;

    /* Copy the data to be written right after the address */
    for (i = 0; i < u32Len; i++)
    {
        u8TxBuf[i + 1] = pu8Data[i];
    }

    /* Call official API for multi-byte write (Total length = data length + 1 byte address) */
    I2C_WriteMultiBytes(I2C0, EEPROM_SLAVE_ADDR, u8TxBuf, u32Len + 1);

    /* Wait for the EEPROM to burn the buffer data into non-volatile memory */
    EEPROM_WaitReady();
}


void EEPROM_WritePage_1(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    /* Create transmit buffer: 1 Byte memory address + up to 16 Bytes of data */
    uint8_t u8TxBuf[17];
    uint32_t i;

    /* Place the internal memory address at index 0 */
    u8TxBuf[0] = u8DataAddr;

    /* Copy the data to be written right after the address */
    for (i = 0; i < u32Len; i++)
    {
        u8TxBuf[i + 1] = pu8Data[i];
    }

    /* Call official API for multi-byte write (Total length = data length + 1 byte address) */
    I2C_WriteMultiBytes(I2C2, EEPROM_SLAVE_ADDR, u8TxBuf, u32Len + 1);

    /* Wait for the EEPROM to burn the buffer data into non-volatile memory */
    EEPROM_WaitReady_1();
}



/**
 * @brief   High-level function: Automatically handles page boundaries, can write up to 256 Bytes
 * @param   u8DataAddr  Starting write address (0x00 ~ 0xFF)
 * @param   pu8Data     Pointer to the data array to be written
 * @param   u32Len      Total length of data to write
 */
void EEPROM_WriteData(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    uint32_t u32WriteLen;
    uint32_t u32PageSpace;

    while (u32Len > 0)
    {
        /* Calculate remaining space in the current page (16 Bytes per page) */
        u32PageSpace = 16 - (u8DataAddr % 16);

        /* Determine how many bytes to write this time (min of remaining len & page space) */
        u32WriteLen = (u32Len < u32PageSpace) ? u32Len : u32PageSpace;

        /* Call the page write function */
        EEPROM_WritePage(u8DataAddr, pu8Data, u32WriteLen);

        /* Update address and pointers for the next iteration */
        u8DataAddr += u32WriteLen;
        pu8Data    += u32WriteLen;
        u32Len     -= u32WriteLen;
    }
}


void EEPROM_WriteData_1(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    uint32_t u32WriteLen;
    uint32_t u32PageSpace;

    while (u32Len > 0)
    {
        /* Calculate remaining space in the current page (16 Bytes per page) */
        u32PageSpace = 16 - (u8DataAddr % 16);

        /* Determine how many bytes to write this time (min of remaining len & page space) */
        u32WriteLen = (u32Len < u32PageSpace) ? u32Len : u32PageSpace;

        /* Call the page write function */
        EEPROM_WritePage_1(u8DataAddr, pu8Data, u32WriteLen);

        /* Update address and pointers for the next iteration */
        u8DataAddr += u32WriteLen;
        pu8Data    += u32WriteLen;
        u32Len     -= u32WriteLen;
    }
}


/**
 * @brief   Sequentially read multiple bytes of data from CAT34TS02 EEPROM
 * @details Directly calls the BSP provided I2C_ReadMultiBytesOneReg to complete the task
 */
void EEPROM_ReadData(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    /* The official API handles the dummy write address and Repeated Start -> Read flows automatically */
    I2C_ReadMultiBytesOneReg(I2C0, EEPROM_SLAVE_ADDR, u8DataAddr, pu8Data, u32Len);
}

void EEPROM_ReadData_1(uint8_t u8DataAddr, uint8_t *pu8Data, uint32_t u32Len)
{
    /* The official API handles the dummy write address and Repeated Start -> Read flows automatically */
    I2C_ReadMultiBytesOneReg(I2C2, EEPROM_SLAVE_ADDR, u8DataAddr, pu8Data, u32Len);
}
