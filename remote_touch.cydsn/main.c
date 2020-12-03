/* ========================================
 *
 * Copyright Nafu Studios, 2020
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF NAFU STUDIOS.
 *
 * ========================================
*/
#include <stdio.h>

#include "project.h"

// RTD resistor divider
#define V_S         5.0       // 5V
#define R_TOP       1000.0    // RTD, 1kOhm at 0C
#define R_BOT       10000.0   // descrete resistor
#define OHM_PER_C   3.85      // RTD ohms per degree C
#define RTD_OFFSET  47.0

float rtd_volt_to_temp(float voltage) {
    return (V_S * R_BOT - voltage * (R_TOP + R_BOT)) / (voltage * OHM_PER_C) + RTD_OFFSET;
}

// Heater controller
#define TARGET_TEMP         34   // selected empirically
#define MIN_HEATER_DUTY     147  // takes ~5min before it's "too hot"

void heater_controller(float temp) {
    if (temp < TARGET_TEMP) {
        PWM_HEATER_WriteCompare(UINT8_MAX);
        char buf[64];
        sprintf(buf, "MAX duty\n");
        UART_PC_PutString(buf);
    } else {
        PWM_HEATER_WriteCompare(MIN_HEATER_DUTY);
        char buf[64];
        sprintf(buf, "min duty\n");
        UART_PC_PutString(buf);
    }
}

int main(void)
{
    CyGlobalIntEnable;

    // Copper heater
    PWM_HEATER_Start();

    // Copper temperature sensor
    ADC_TEMP_Start();
    ADC_TEMP_StartConvert();
    float temp_copper = 0;

    // Serial debugging
    UART_PC_Start();

    // Initialize Touch Sensor
    CyDelay(2000);
    CapSense_CSD_Start();
    CapSense_CSD_InitializeAllBaselines();
    uint8_t is_touched = 0;

    for(;;) {

        // Read RTD temp
        if (ADC_TEMP_IsEndConversion(ADC_TEMP_RETURN_STATUS)) {
            uint32_t adc_RTD0 = ADC_TEMP_GetResult32();
            temp_copper = rtd_volt_to_temp(ADC_TEMP_CountsTo_Volts(adc_RTD0));
        }

        // Print out temperatures
        char buf[64];
        sprintf(buf, "RTD %.2f\n", temp_copper);
        UART_PC_PutString(buf);

        // Read touch sensor
        if(0u == CapSense_CSD_IsBusy()) {
            CapSense_CSD_UpdateEnabledBaselines();
            CapSense_CSD_ScanEnabledWidgets();
            is_touched = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_TOUCH0__BTN);
        }

        if (is_touched) heater_controller(temp_copper);
        else PWM_HEATER_WriteCompare(0);
    }
}

/* [] END OF FILE */
