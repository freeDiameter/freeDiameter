/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2012, WIDE Project and NICT								 *
* All rights reserved.											 *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/

#include "fdproto-internal.h"

/* Replacement for clock_gettime for the Mac OS
 *  NOTE: macOS 10.12 (Sierra) and later natively support clock_gettime.
 *  It is highly recommended to build the fD framework on macOS 10.12 or newer.
*/
#ifndef HAVE_CLOCK_GETTIME
int clock_gettime(int clk_id, struct timespec* ts)
{
	if (clk_id == CLOCK_REALTIME) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = tv.tv_usec * 1000;
	}
#ifdef __APPLE__
	else if (clk_id == CLOCK_MONOTONIC) {
		static mach_timebase_info_data_t timebase = {0, 0};
		uint64_t time;
		uint64_t nsec;

		if (timebase.denom == 0) {
			mach_timebase_info(&timebase);
		}

		time = mach_absolute_time();
		nsec = time * timebase.numer / timebase.denom;

		ts->tv_sec = nsec / 1000000000;
		ts->tv_nsec = nsec % 1000000000;
	}
#endif
	else {
		errno = EINVAL;
		return -1;
	}
    
	return 0;
}
#endif /* HAVE_CLOCK_GETTIME */

/* Fallback function for using "pthread_cond_timedwait" with no "pthread_condattr_setclock" support (Mac OS) */
#ifndef HAVE_PTHREAD_CONDATTR_SETCLOCK
int pthread_cond_timedwait_fb (pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
	int ret = 0;
	struct timespec now, chunk, realtime_now, abs_realtime;
	
	while (1) {
		CHECK_SYS_DO( clock_gettime(CLOCK_MONOTONIC, &now), {ret = EINVAL; break;} );
		time_t sec_left = abstime->tv_sec - now.tv_sec;
		long nsec_left = abstime->tv_nsec - now.tv_nsec;
		if (nsec_left < 0) {
			sec_left--;
			nsec_left += 1000000000;
		}

		if (sec_left < 0 || (sec_left == 0 && nsec_left <= 0)) {
			ret = ETIMEDOUT;
			break;
		}

		if (sec_left > (FB_TM_INTERVAL_MS / 1000) || 
			(sec_left == (FB_TM_INTERVAL_MS / 1000) && (nsec_left / 1000000) > (FB_TM_INTERVAL_MS % 1000))) {
			chunk.tv_sec = FB_TM_INTERVAL_MS / 1000;
			chunk.tv_nsec = (FB_TM_INTERVAL_MS % 1000) * 1000000;
		} else {
			chunk.tv_sec = sec_left;
			chunk.tv_nsec = nsec_left;
		}

		CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &realtime_now), {ret = EINVAL; break;} );
		abs_realtime.tv_sec = realtime_now.tv_sec + chunk.tv_sec;
		abs_realtime.tv_nsec = realtime_now.tv_nsec + chunk.tv_nsec;
		if (abs_realtime.tv_nsec >= 1000000000) {
			abs_realtime.tv_sec++;
			abs_realtime.tv_nsec -= 1000000000;
		}

		ret = pthread_cond_timedwait(cond, mutex, &abs_realtime);

		if (ret == ETIMEDOUT) {
			continue;
		}

		break;
	}
	
	return ret;
}
#endif /* HAVE_PTHREAD_CONDATTR_SETCLOCK */

/* Replacement for strndup for the Mac OS */
#ifndef HAVE_STRNDUP
char * strndup (char *str, size_t len)
{
	char * output;
	size_t outlen;
	
	output = memchr(str, 0, len);
	if (output == NULL) {
		outlen = len;
	} else {
		outlen = output - str;
	}
	
	CHECK_MALLOC_DO( output = malloc (outlen + 1), return NULL );

	output[outlen] = '\0';
	memcpy (output, str, outlen);
	return output;
}
#endif /* HAVE_STRNDUP */
