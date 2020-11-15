#include "ui.h"
#include "led7seg.h"
#include "controller.h"
#include "drv_adc.h"
#include "drv_tmp117.h"
#include <stdint.h>
#include <math.h>

//Tuning for LPF based on Ts = 0.01
#define FILTER_A_01Hz		(0.9937365126247782f)
#define FILTER_A_05Hz		(0.9690724263048106f)
#define FILTER_A_1Hz		(0.9391013674242926f)
#define FILTER_A_2Hz		(0.8819113782981763f)
#define FILTER_A_5Hz		(0.7304026910486456f)
#define FILTER_A_10Hz		(0.5334880910911033f)
#define FILTER_A_20Hz		(0.2846095433360293f)
#define FILTER_A_30Hz		(0.1518358019806489f)
inline static float __filter(float filter_gain_A, float input, float *prev_input);

static float latest_tempF_z1 = 0.0f;
static float latest_tempF = 0.0f;

static float tempF_ref_z1 = 0.0f;
static float tempF_ref_prev = 0.0f;
static float tempF_ref = 0.0f;

static bool is_first_time = true;

// True when user is adjusting slider
static bool is_mode_setting_slider = false;
static bool is_mode_ui_off = false;
static uint32_t idle_counter = 0;

static float __read_slider_tempF(void)
{
	static const float OUT_GAIN = 10.0f / 4095.0f;
	float ret = 65.0f + (OUT_GAIN * (float)drv_adc_sample());

	// Round to nearest integer degree
	ret = roundf(ret);

	return ret;
}

void ui_step(void)
{
	// Seed the filters on the first run
	if (is_first_time) {
		is_first_time = false;

		latest_tempF_z1 = drv_tmp117_readTempF() * FILTER_A_01Hz;

		tempF_ref_prev = __read_slider_tempF();
		tempF_ref_z1 = tempF_ref_prev * FILTER_A_2Hz;
	}

	// Read latest temp from sensor and filter
    latest_tempF = __filter(FILTER_A_01Hz, drv_tmp117_readTempF(), &latest_tempF_z1);

	// Read raw input from slider and filter
	tempF_ref = __filter(FILTER_A_2Hz, __read_slider_tempF(), &tempF_ref_z1);

	bool is_user_touching_device = fabsf(tempF_ref - tempF_ref_prev) >= 0.01f;
	if (is_user_touching_device) {
		idle_counter = 0;
	} else {
		idle_counter++;
	}

	is_mode_setting_slider = idle_counter < 5*100; // 5 seconds
	is_mode_ui_off = false && idle_counter > 30*100; // 30 seconds

	// Handle UI state
	if (is_mode_setting_slider) {
		// User is adjusting device!
		led7seg_set_brightness(5000);
		led7seg_show_float(tempF_ref);
	} else if (!is_mode_ui_off) {
		// Device is just regulating temperature...

		led7seg_set_brightness(1000);
		led7seg_show_float(latest_tempF);

		// Update control reference
		controller_set_reference(tempF_ref);
	} else {
		// Turn LEDs off
		led7seg_set_brightness(0);

		// Update control reference
		controller_set_reference(tempF_ref);
	}

	// Update previous states
	tempF_ref_prev = tempF_ref;
}

float ui_get_latest_tempF(void)
{
	return latest_tempF;
}

void ui_welcome_screen(void)
{
	led7seg_set_brightness(9000);

	led7seg_show(
			't', 0,
			'o', 0,
			'H', 0
			);
}

inline static float __filter(float filter_gain_A, float input, float *z1)
{
    float output = (input * (1.0f - filter_gain_A)) + *z1;
    *z1 = output * filter_gain_A;
    return output;
}
