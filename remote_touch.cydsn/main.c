#include <stdio.h>

#include "project.h"

// Get current Timer count
uint32_t stopwatch_start(void) {
    return Timer_ReadCounter();
}

// Returns elapsed time since count (assumes 1kHz count clock)
uint32_t stopwatch_elapsed_ms(uint32_t timer_count) {
    uint32_t curr_time_ms = Timer_ReadCounter();
    if (timer_count >= curr_time_ms) return (timer_count - curr_time_ms);
    // Handle counter rollover
    return (timer_count + (Timer_ReadPeriod() - curr_time_ms));
}

// Cooling timer
#define MAX_COOLING_TIME_MS     7000
uint32_t timer_cooling = MAX_COOLING_TIME_MS;
uint32_t timer_heating = 0;

// RTD resistor divider
#define V_S         5.0       // 5V
#define R_TOP       1000.0    // RTD, 1kOhm at 0C
#define R_BOT       10000.0   // descrete resistor
#define OHM_PER_C   3.85      // RTD ohms per degree C
//#define RTD_OFFSET  47.0      // Frame 1
#define RTD_OFFSET  32.0      // Frame 2

// Convert RTD voltage to temp (units C)
float rtd_volt_to_temp(float voltage) {
    return (V_S * R_BOT - voltage * (R_TOP + R_BOT)) / (voltage * OHM_PER_C) + RTD_OFFSET;
}

// Heater controller
#define TARGET_TEMP         30    // selected empirically
#define MIN_HEATER_DUTY     147   // takes ~5min before it's "too hot"

#define RELAY_SW_TIME_MS    5     // max relay switching time
#define RELAY_CLICK_FREQ    1000  // freq, in ms, of relay clicking

// Fast heat up and then keep steady
void heater_controller(float temp) {
    // Make sure peltiers are inactive (relays set to same state)
    if (!PIN_P_COOL_Read() != !PIN_P_HEAT_Read()) {
        PIN_P_COOL_Write(0);
        PIN_P_HEAT_Write(0);
        CyDelay(RELAY_SW_TIME_MS);
    }

    // Click peltier relays for audible signal if remote touch until local touch
    if (!PIN_IS_TOUCHED_Read()) {
        static uint32_t timer_click = 0;
        if (stopwatch_elapsed_ms(timer_click) > RELAY_CLICK_FREQ) {
            PIN_P_COOL_Write(!PIN_P_COOL_Read());
            PIN_P_HEAT_Write(!PIN_P_HEAT_Read());
            timer_click = stopwatch_start();
        }
    }

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

    timer_cooling = stopwatch_start();  // reset cooling timer
}

// Cools for a set duration
void cooling_controller() {
    // Make sure heater is off
    if (PWM_HEATER_ReadCompare()) {
        PWM_HEATER_WriteCompare(0);
        CyDelay(1);
    }

    // Make sure heater relay is off
    if (PIN_P_HEAT_Read()) {
        PIN_P_HEAT_Write(0);
        CyDelay(RELAY_SW_TIME_MS);
    }

    // Cool only for a set duration
    if (stopwatch_elapsed_ms(timer_cooling) < MAX_COOLING_TIME_MS) {
        PIN_P_COOL_Write(1);
        char buf[64];
        sprintf(buf, "COOLING\n");
        UART_PC_PutString(buf);
    } else {
        PIN_P_COOL_Write(0);
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

    Timer_Start();

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
            PIN_IS_TOUCHED_Write(is_touched);
        }

        if (PIN_REMOTE_TOUCH_Read()) heater_controller(temp_copper);
        else cooling_controller();
    }
}

/* [] END OF FILE */
