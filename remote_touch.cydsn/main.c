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
#include "project.h"

// RTD resistor divider
#define V_S         5.0       // 5V
#define R_TOP       1000.0    // RTD, 1kOhm at 0C
#define R_BOT       10000.0   // descrete resistor
#define OHM_PER_C   3.85      // RTD ohms per degree C
#define RTD_OFFSET  30.0

float rtd_volt_to_temp(float voltage) {
    return (V_S * R_BOT - voltage * (R_TOP + R_BOT)) / (voltage * OHM_PER_C) + RTD_OFFSET;
}

int main(void)
{
    CyGlobalIntEnable;

    // Copper heater
    PWM_HEATER_Start();

    // Copper temperature sensor
    ADC_TEMP_Start();
    ADC_TEMP_StartConvert();
    float temp_copper;

    for(;;) {

        if (ADC_TEMP_IsEndConversion(ADC_TEMP_RETURN_STATUS)) {
            uint32_t adc_RTD1 = ADC_TEMP_GetResult32();
            temp_copper = rtd_volt_to_temp(ADC_TEMP_CountsTo_Volts(adc_RTD1));
        }
    }
}

/* [] END OF FILE */
