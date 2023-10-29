#include <stdio.h>

#include "project.h"

// Uncomment to communicate with Tuner GUI
//#define EZTUNE

// for status messages
#define STATE_IDLE  0
#define STATE_HEAT  1
#define STATE_COOL  2
#define NUM_STATES  3

const char* state_names[NUM_STATES] = {
    [STATE_IDLE] = "idle...",
    [STATE_HEAT] = "heating",
    [STATE_COOL] = "cooling"
};

uint32_t state = STATE_IDLE;

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
#define MAX_COOLING_TIME_MS     60000
uint32_t timer_cooling = MAX_COOLING_TIME_MS;

// RTD resistor divider
#define V_S         5.0       // 5V
#define R_TOP       1000.0    // RTD, 1kOhm at 0C
#define R_BOT       10000.0   // descrete resistor
#define OHM_PER_C   3.85      // RTD ohms per degree C
#define RTD_OFFSET  47.0      // Frame Client
//#define RTD_OFFSET  32.0      // Frame Server

// Convert RTD voltage to temp (units C)
float rtd_volt_to_temp(float voltage) {
    return (V_S * R_BOT - voltage * (R_TOP + R_BOT)) / (voltage * OHM_PER_C) + RTD_OFFSET;
}

// Heater controller
#define TARGET_TEMP         30    // selected empirically
#define MIN_HEATER_DUTY     147   // takes ~5min before it's "too hot"

// Relay timings
#define RELAY_SW_TIME_MS    5     // max relay switching time
#define RELAY_CLICK_FREQ    1000  // freq, in ms, of relay clicking

// Controls heating (heats up fast and then keeps steady)
void heater_controller(float temp) {
    state = STATE_HEAT;

    // Make sure peltiers are inactive (relays set to same state)
    if (!PIN_P_COOL_Read() != !PIN_P_HEAT_Read()) {
        PIN_P_COOL_Write(0);
        PIN_P_HEAT_Write(0);
        CyDelay(RELAY_SW_TIME_MS);
    }

    // Click peltier relays for audible signal unless touched locally
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
    } else {
        PWM_HEATER_WriteCompare(MIN_HEATER_DUTY);
    }

    timer_cooling = stopwatch_start();  // reset cooling timer
}

// Control cooling (cools for a set duration)
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

    // Cool for a set duration to avoid overheating heatsink
    if (stopwatch_elapsed_ms(timer_cooling) < MAX_COOLING_TIME_MS) {
        state = STATE_COOL;
        PIN_P_COOL_Write(1);
    } else {
        state = STATE_IDLE;
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
#ifdef EZTUNE
    CapSense_CSD_TunerStart();
#else
    CapSense_CSD_Start();
    CapSense_CSD_InitializeAllBaselines();
    CapSense_CSD_InitializeAllBaselines();
    uint8_t is_touched = 0;
#endif

    Timer_Start();

    for(;;) {

        // Read RTD temp
        if (ADC_TEMP_IsEndConversion(ADC_TEMP_RETURN_STATUS)) {
            uint32_t adc_RTD0 = ADC_TEMP_GetResult32();
            temp_copper = rtd_volt_to_temp(ADC_TEMP_CountsTo_Volts(adc_RTD0));
        }

#ifdef EZTUNE
        CapSense_CSD_TunerComm();
#else
        // Read touch sensor
        if(0u == CapSense_CSD_IsBusy()) {
            CapSense_CSD_UpdateEnabledBaselines();
            CapSense_CSD_ScanEnabledWidgets();
            is_touched = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_TOUCH0__BTN);
            PIN_IS_TOUCHED_Write(is_touched);
        }
#endif
        // Heat or cool depending on remote touch state
        if (PIN_REMOTE_TOUCH_Read()) heater_controller(temp_copper);
        else cooling_controller();

        // print out status message
        char buf[64];
        sprintf(buf, "%s | %03d duty | RTD %.2f\n", state_names[state], PWM_HEATER_ReadCompare(), temp_copper);
        UART_PC_PutString(buf);
    }
}