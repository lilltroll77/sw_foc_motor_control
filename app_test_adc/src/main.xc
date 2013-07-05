/**
 * The copyrights, all other intellectual and industrial 
 * property rights are retained by XMOS and/or its licensors. 
 * Terms and conditions covering the use of this code can
 * be found in the Xmos End User License Agreement.
 *
 * Copyright XMOS Ltd 2013
 *
 * In the case where this code is a modification of existing code
 * under a separate license, the separate license terms are shown
 * below. The modifications to the code are still covered by the 
 * copyright notice above.
 **/                                   

#include "main.h"

// ADC ports
on tile[MOTOR_TILE]: buffered in port:32 pb32_adc_data[NUM_ADC_DATA_PORTS] = { PORT_ADC_MISOA ,PORT_ADC_MISOB }; // NB For Phase_A and Phase_B
on tile[MOTOR_TILE]: port p1_adc_ready = PORT_ADC_CONV; // bi-directional 1-bit port as used to as Input ready signal for pb32_adc_data ports, and Output port to ADC chip
on tile[MOTOR_TILE]: out port p1_adc_sclk = PORT_ADC_CLK; // 1-bit port connecting to external ADC serial clock
on tile[MOTOR_TILE]: out port p4_adc_mux = PORT_ADC_MUX; // 4-bit port used to control multiplexor on ADC chip
on tile[MOTOR_TILE]: clock adc_xclk = XS1_CLKBLK_2; // Internal XMOS clock

// Test ports (Borrowed from Motor_0 PWM hi-leg)
on tile[MOTOR_TILE]: buffered out port:32 pb32_tst_data[NUM_ADC_DATA_PORTS]	= {	PORT_M1_HI_A, PORT_M1_HI_B }; // NB For Phase_A and Phase_B
on tile[MOTOR_TILE]: in port p1_tst_ready = PORT_M1_LO_C; // 1-bit Input port as used to as ready signal for pb32_tst_data ports
on tile[MOTOR_TILE]: in port p1_tst_sclk = PORT_M1_HI_C; // 1-bit port receives serial clock
on tile[MOTOR_TILE]: clock tst_xclk = XS1_CLKBLK_3; // Internal XMOS clock

/*****************************************************************************/
int main ( void ) // Program Entry Point
{
	chan c_pwm2adc_trig[NUMBER_OF_MOTORS]; // Channel for sending PWM-to_ADC trigger pulse
	streaming chan c_adc_chk[NUMBER_OF_MOTORS]; // Array of channel for communication between ADC_Server & Checker cores
	streaming chan c_tst_chk; // Channel for communication between Test_Generator & Checker cores
	streaming chan c_tst_sin; // Channel for communication between Test_Generator & Sine_Generator cores
	streaming chan c_adc_sin; // Channel for communication between ADC_Interface & Sine_Generator cores
	streaming chan c_sin_chk; // Channel for communication between Checker & Sine_Generator cores


	par
	{	// NB All cores are run on one tile so that all cores use the same clock frequency (250 MHz)
		on tile[MOTOR_TILE] : 
		{
		  init_locks(); // Initialise Mutex for display

			par
			{
				gen_all_adc_test_data( c_tst_chk ,c_tst_sin ); // Generate test data

				get_sine_data( c_tst_sin ,c_sin_chk ,c_adc_sin ); // Generates a Sine value having received a velocity and time value

				// I/O via H/W specific interface
#if (1 == HW_ADC_7265)
				adc_7265_interface( c_pwm2adc_trig ,c_adc_sin ,pb32_tst_data ,p1_tst_ready ,p1_tst_sclk ,tst_xclk ); // Simulate ADC_7265 I/F

				// ADC_7265 Server under test		
				foc_adc_7265_triggered( c_adc_chk ,c_pwm2adc_trig ,pb32_adc_data ,adc_xclk ,p1_adc_sclk ,p1_adc_ready ,p4_adc_mux );
#endif // (1 == HW_ADC_7265)
		
				check_all_adc_client_data( c_adc_chk ,c_sin_chk ,c_tst_chk ); // Check results using ADC Client
			} // par
		
		  free_locks(); // Free Mutex for display
		} // on tile[MOTOR_TILE] : 
	} // par 

	return 0;
} // main
/*****************************************************************************/
// main.xc
