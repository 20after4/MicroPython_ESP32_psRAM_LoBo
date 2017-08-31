/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "rom/uart.h"

#include "py/obj.h"
#include "py/mpstate.h"
#include "py/mphal.h"
#include "extmod/misc.h"
#include "lib/utils/pyexec.h"
#include "uart.h"
#include "sdkconfig.h"

#ifdef CONFIG_MICROPY_USE_TELNET
#include "telnet.h"
#endif

STATIC uint8_t stdin_ringbuf_array[CONFIG_MICROPY_RX_BUFFER_SIZE];
ringbuf_t stdin_ringbuf = {stdin_ringbuf_array, sizeof(stdin_ringbuf_array), 0, 0};

// wait until at least one character is received or the timeout expires
//---------------------------------------
int mp_hal_stdin_rx_chr(uint32_t timeout)
{
	uint32_t wait_end = mp_hal_ticks_ms() + timeout;
	int c = -1;

    for (;;) {
    	if (mp_hal_ticks_ms() > wait_end) return -1;

		#ifdef CONFIG_MICROPY_USE_TELNET
		// read telnet first
		if (telnet_rx_any()) return telnet_rx_char();
		#endif

		c = ringbuf_get(&stdin_ringbuf);
    	if (c < 0) {
    		// no character in ring buffer
        	// wait 10 ms for character
    	   	MP_THREAD_GIL_EXIT();
        	if ( xSemaphoreTake( uart0_semaphore, 10 / portTICK_PERIOD_MS ) == pdTRUE ) {
        	   	MP_THREAD_GIL_ENTER();
                c = ringbuf_get(&stdin_ringbuf);
        	}
        	else {
        	   	MP_THREAD_GIL_ENTER();
        		c = -1;
        	}
    	}
    	if (c >= 0) {
    		// Character received
			#ifdef CONFIG_MICROPY_USE_TELNET
    		if (telnet_loggedin()) {
    			if (c == 20) {
    				// Ctrl_T received, reset telnet
    				telnet_reset();
    	        	printf("[Telnet] Connection terminated from REPL\n");
    			}
	            return -1;
    		}
			#endif
    		return c;
    	}

        xSemaphoreTake(uart0_mutex, UART_SEMAPHORE_WAIT);
        int raw = uart0_raw_input;
    	xSemaphoreGive(uart0_mutex);
        if (raw == 0) {
        	MICROPY_EVENT_POLL_HOOK
        }
    }
    return -1;
}

#ifdef CONFIG_MICROPY_USE_TELNET
// Convert '\n' to '\r\n'
//-------------------------------------------------------------
static void telnet_stdout_tx_str(const char *str, uint32_t len)
{
	char *tstr = malloc(len*2+1);
	char prev = '\0';
	uint32_t idx = 0;
	while (len--) {
		if ((*str == '\n') && (prev != '\r')) tstr[idx++] = '\r';
		tstr[idx++] = *str;
		prev = *str++;
	}
	tstr[idx++] = '\0';
	telnet_tx_strn(tstr, strlen(tstr));
	free(tstr);
}
#endif

//------------------------------------------
void mp_hal_stdout_tx_str(const char *str) {
	#ifdef CONFIG_MICROPY_USE_TELNET
   	if (telnet_loggedin()) telnet_tx_strn(str, strlen(str));
   	else {
   	   	//MP_THREAD_GIL_EXIT();
   	    while (*str) {
   	        uart_tx_one_char(*str++);
   	    }
   	   	//MP_THREAD_GIL_ENTER();
   	}
	#else
   	MP_THREAD_GIL_EXIT();
    while (*str) {
        uart_tx_one_char(*str++);
    }
   	MP_THREAD_GIL_ENTER();
	#endif
}

//---------------------------------------------------------
void mp_hal_stdout_tx_strn(const char *str, uint32_t len) {
	#ifdef CONFIG_MICROPY_USE_TELNET
   	if (telnet_loggedin()) telnet_tx_strn(str, len);
   	else {
   	   	//MP_THREAD_GIL_EXIT();
   	    while (len--) {
   	        uart_tx_one_char(*str++);
   	    }
   		//MP_THREAD_GIL_ENTER();
   	}
	#else
   	MP_THREAD_GIL_EXIT();
    while (len--) {
        uart_tx_one_char(*str++);
    }
	MP_THREAD_GIL_ENTER();
	#endif
}

//----------------------------------------------------------------
void mp_hal_stdout_tx_strn_cooked(const char *str, uint32_t len) {
	#ifdef CONFIG_MICROPY_USE_TELNET
   	if (telnet_loggedin()) telnet_stdout_tx_str(str, len);
   	else {
   	   	//MP_THREAD_GIL_EXIT();
   	    while (len--) {
   	        if (*str == '\n') {
   	            uart_tx_one_char('\r');
   	        }
   	        uart_tx_one_char(*str++);
   	    }
   	   	//MP_THREAD_GIL_ENTER();
   	}
	#else
   	MP_THREAD_GIL_EXIT();
    while (len--) {
        if (*str == '\n') {
            uart_tx_one_char('\r');
        }
        uart_tx_one_char(*str++);
    }
   	MP_THREAD_GIL_ENTER();
	#endif
}

//------------------------------
uint32_t mp_hal_ticks_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

//------------------------------
uint32_t mp_hal_ticks_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

//---------------------------------
void mp_hal_delay_ms(uint32_t ms) {
	if (ms == 0) return;

	uint32_t wait_ticks = ms / portTICK_PERIOD_MS;	// number of ticks in delay time
	uint32_t dticks = ms % portTICK_PERIOD_MS;		// remaining milli seconds

	if (wait_ticks > 0) {
	   	MP_THREAD_GIL_EXIT();
        vTaskDelay(wait_ticks);
       	MP_THREAD_GIL_ENTER();
	}
    // do the remaining delay accurately
    ets_delay_us(dticks * 1000);
}

//---------------------------------
void mp_hal_delay_us(uint32_t us) {
    ets_delay_us(us);
}

// this function could do with improvements (eg use ets_delay_us)
//--------------------------------------
void mp_hal_delay_us_fast(uint32_t us) {
    uint32_t delay = ets_get_cpu_frequency() / 19;
    while (--us) {
        for (volatile uint32_t i = delay; i; --i) {
        }
    }
}

/*
extern int mp_stream_errno;
int *__errno() {
    return &mp_stream_errno;
}
*/
