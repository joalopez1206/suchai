/**
 * @file  osDelay.h
 * @author Carlos Gonzalez Cortes
 * @author Ignacio Ibanez Aliaga
 * @date 2020
 * @copyright GNU Public License.
 *
 * Functions related to time for operating systems Linux and FreeRTOS
 * 
 */

#ifndef _OS_DELAY_H_
#define _OS_DELAY_H_

#include "os/os.h"
#include "stdint.h"
#include "suchai/config.h"
#include "suchai/cpu.h"

#ifdef FREERTOS
	#include "FreeRTOS.h"
	#include "task.h"
	typedef portTickType portTick;  // Always ticks counts (int)
#else
    #include <unistd.h>
    #include <sys/time.h>
    #include <pthread.h>
 	typedef  __useconds_t portTick; // Always useconds (uint32_t)
#endif

/**
 * Translate time in milliseconds to system tick type.
 * @param mseconds uint32_t. Time in milliseconds
 * @return portTick
 */
portTick osDefineTime(uint32_t mseconds);

/**
 * Get system current tick count
 * @return portTick
 */
portTick osTaskGetTickCount(void);

/**
 * Manually set the system tick.
 * The new_tick_us is directly set, not added.
 * Useful when used in simulator environment to control system tick.
 * @param new_tick_us New tick.
 */
void osTaskSetTickCount(portTick new_tick_us);

/**
 * Delay task execution
 * @param milliseconds uint32_t. Milliseconds to sleep
 */
void osDelay(uint32_t mseconds);

/**
 * Delay task execution in given millisecond since last delay. Use this function
 * to fine control periodic tasks.
 * @param lastTime portTick. Tick count of the last delay
 * @param milliseconds Long. Milliseconds to sleep.
 */
void osTaskDelayUntil(portTick *lastTime, uint32_t mseconds);

/**
 * Set current system time
 * @param time Unix time (64 bit integer)
 * @return 1 if ok, 0 on error
 */
int osSetTimeUnix(int64_t time);

/**
 * Get current system time formatted as 64 bit integer Unix time
 * @return Unix time
 */
int64_t osGetTimeUnix(void);

#endif
