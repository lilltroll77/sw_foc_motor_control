/*
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
 */                                   

/*****************************************************************************\
	This code is designed to work on a Motor with a Max speed of 4000 RPM,
	and a 1024 counts per revolution.

	The QEI data is read in via a 4-bit port. With assignments as follows:-

	 bit_3   bit_2   bit_1    bit_0
	-------  -----  -------  -------
  N_Error  Index  Phase_B  Phase_A

	N_Error = 1 for No Errors

	In normal operation the B and A bits change as a grey-code,
	with the following convention

			  ----------->  Counter-Clockwise
	BA:  00 01 11 10 00
			  <-----------  Clockwise

	During one revolution, BA will change 1024 times,
	Index will take the value of zero 1023 times, and the value one once only,
  at the position origin. 
	NB When the motor starts, it is NOT normally at the origin

	A look-up table is used to decode the 2 phase bits, into a spin direction
	with the following meanings: 
		 1: Anit-Clocwise, 
		 0: Unknown    (The motor has either stopped, or jumped one or more phases)
		-1: Clocwise, 

	The timer is read every time the phase bits change. I.E. 1024 times per revolution

	The angular postion is incremented/decremented (with the spin value) if the 
	motor is NOT at the origin. 
	If the motor is at the origin, the angular position is reset to zero.

\*****************************************************************************/

#ifndef _QEI_SERVER_H_
#define _QEI_SERVER_H_

#include <stdlib.h>

#include <xs1.h>
#include <assert.h>
#include <print.h>

#include "qei_common.h"

#ifndef QEI_FILTER
	#error Define. QEI_FILTER in app_global.h
#endif

#define HALF_QEI_CNT (QEI_PER_REV >> 1) // 180 degrees of mechanical rotation

#ifndef MAX_SPEC_RPM 
	#error Define. MAX_SPEC_RPM in app_global.h
#endif // MAX_SPEC_RPM

#define MIN_TICKS_PER_QEI (TICKS_PER_MIN_PER_QEI / MAX_SPEC_RPM) // Min. expected Ticks/QEI // 12 bits
#define THR_TICKS_PER_QEI (MIN_TICKS_PER_QEI >> 1) // Threshold value used to trap annomalies // 11 bits

#define MAX_CONFID 4 // Maximum confidence value
#define MAX_QEI_STATE_ERR 8 // Maximum number of consecutive QEI state-transition errors allowed

#define QEI_CNT_LIMIT (QEI_PER_REV + HALF_QEI_CNT) // 540 degrees of rotation

#define START_UP_CHANGES 3 // Must see this number of pin changes before calculating velocity

#define QEI_SCALE_BITS 16 // Used to generate 2^n scaling factor
#define QEI_HALF_SCALE (1 << (QEI_SCALE_BITS - 1)) // Half Scaling factor (used in rounding)

#define QEI_COEF_BITS 8 // Used to generate filter coef divisor. coef_div = 1/2^n
#define QEI_COEF_DIV (1 << QEI_COEF_BITS) // Coef divisor
#define QEI_HALF_COEF (QEI_COEF_DIV >> 1) // Half of Coef divisor

#define MAX_QEI_STATUS_ERR 3  // Maximum number of consecutive QEI status errors allowed

#define MIN_RPM 50 // In order to estimate the angular position, a minimum expected RPM has to be specified
// Now we can calculate the maximum expected time difference (in ticks) between QEI phase changes
#define MAX_TIME_DIFF (((MIN_RPM * TICKS_PER_SEC_PER_QEI) + (SECS_PER_MIN - 1)) / SECS_PER_MIN) // Round-up maximum expected time-diff (17-bits)

typedef signed long long S64_T; //MB~ Put this in app_global.h
typedef unsigned long long U64_T; //MB~ Put this in app_global.h

#define INT16_BITS (sizeof(short) * BITS_IN_BYTE) // No. of bits in 16-bit integer
#define INT32_BITS (sizeof(int) * BITS_IN_BYTE) // No. of bits in 32-bit integer
#define INT64_BITS (sizeof(S64_T) * BITS_IN_BYTE) // No. of bits in signed 64-bit type!

/** Different Motor Phases */
typedef enum QEI_ENUM_TAG
{
  QEI_ANTI = -1,		// Anti-Clockwise Phase change
  QEI_STALL = 0,  // Same Phase
  QEI_CLOCK = 1, // Clockwise Phase change
  QEI_JUMP = 2,		// Jumped 2 Phases
} QEI_ENUM_TYP;

typedef signed char ANG_INC_TYP; // Angular Increment type

/** Structure containing QEI parameters for one motor */
typedef struct QEI_DATA_TAG // 
{
	QEI_PARAM_TYP params; // QEI Parameter data (sent to QEI Client)
	QEI_PHASE_TYP inv_phase;	// Structure containing all inverse QEI phase values;
	unsigned inp_pins; // Raw data values on input port pins
	unsigned prev_phases; // Previous phase values
	unsigned curr_time; // Time when port-pins read
	unsigned prev_time; // Previous port time-stamp
	unsigned diff_time; // Difference between 2 adjacent time-stamps. NB Must be unsigned due to clock-wrap 
	unsigned interval; // expected interval between QEI phase changes
	int t_dif_new; // newest difference between 2 adjacent time-stamps (down-scaled). NB Must be unsigned due to clock-wrap 
	unsigned t_dif_cur; // current difference between 2 adjacent time-stamps. NB Must be unsigned due to clock-wrap 
	unsigned t_dif_old; // oldest difference between 2 adjacent time-stamps. NB Must be unsigned due to clock-wrap 
	U64_T squ_diff; // Square of current time difference
	U64_T prev_squ; // previous square of time-difference
	U64_T numr_64;// Current 64-bit Numerator
	S64_T divr_64; // Current 64-bit Divisor
	S64_T tst_64;// test value for nearest estimate comparison 
	int scale_bits; // Bit-shift used when down-scaling
	unsigned half_scale; // Used to round when down-scaling
	U64_T max_thr; // down-scaling threshold
	QEI_ENUM_TYP prev_state; // Previous QEI state
	int state_errs; // counter for invalid QEI state transistions
	int status_errs; // counter for invalid QEI status errors
	int ang_cnt; // Counts angular position of motor (from origin)
	int prev_ang; // MB~
	ANG_INC_TYP ang_inc; // angular increment value
	int theta; // angular position returned to client
	int spin_sign; // Sign of spin direction
	int ang_speed; // Angular speed of motor measured in Ticks/angle_position
	int prev_orig; // Previous origin flag
	int confid; // Spin-direction confidence. (+ve: confident Clock-wise, -ve: confident Anti-clockwise)
	int id; // Unique motor identifier
	int dbg; // Debug

	int filt_val; // filtered value
	int coef_err; // Coefficient diffusion error
	int scale_err; // Scaling diffusion error 
	int speed_err; // Speed diffusion error 
} QEI_DATA_TYP;

/*****************************************************************************/
/** \brief Get QEI Sensor data from port (motor) and send to client
 * \param c_qei // Array of channels connecting server & client
 * \param p4_qei // Array of QEI data ports for each motor
 */
void foc_qei_do_multiple( // Get QEI Sensor data from port (motor) and send to client
	streaming chanend c_qei[], // Array of channels connecting server & client
	port in p4_qei[] // Array of QEI data ports for each motor
);
/*****************************************************************************/

#endif // _QEI_SERVER_H_