/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2010, WIDE Project and NICT								 *
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

#include "tests.h"

#define TEST_DIAM_ID 	"testsess.myid"
#define TEST_OPT	"suffix"
#define TEST_SID	TEST_DIAM_ID ";1234;5678;" TEST_OPT

#define TEST_EYEC	0x7e57e1ec
struct mystate {
	int	eyec;	/* TEST_EYEC */
	char *  sid; 	/* the session with which the data was registered */
	int  *  freed;	/* location where to write the freed status */
};

static void mycleanup( struct mystate * data, char * sid )
{
	/* sanity */
	CHECK( 1, sid ? 1 : 0 );
	CHECK( 1, data? 1 : 0 );
	CHECK( TEST_EYEC, data->eyec );
	CHECK( 0, strcmp(sid, data->sid) );
	if (data->freed)
		*(data->freed) += 1;
	/* Now, free the data */
	free(data->sid);
	free(data);
}

static __inline__ struct mystate * new_state(char * sid, int *freed) 
{
	struct mystate *new;
	new = malloc(sizeof(struct mystate));
	CHECK( 1, new ? 1 : 0 );
	memset(new, 0, sizeof(struct mystate));
	new->eyec = TEST_EYEC;
	new->sid = strdup(sid);
	CHECK( 1, new->sid ? 1 : 0 );
	new->freed = freed;
	return new;
}
	

/* Main test routine */
int main(int argc, char *argv[])
{
	struct session_handler * hdl1, *hdl2;
	struct session *sess1, *sess2, *sess3;
	char *str1, *str2;
	int new;
	
	/* First, initialize the daemon modules */
	INIT_FD();
	
	/* Test functions related to handlers (simple situation) */
	{
		CHECK( 0, fd_sess_handler_create ( &hdl1, mycleanup ) );
		CHECK( 0, fd_sess_handler_create ( &hdl2, mycleanup ) );
		CHECK( 0, fd_sess_handler_destroy( &hdl2 ) );
		CHECK( 0, fd_sess_handler_create ( &hdl2, mycleanup ) );
		#if 0
		fd_sess_dump_hdl(0, hdl1);
		fd_sess_dump_hdl(0, hdl2);
		#endif
	}
	
	/* Test Session Id generation (fd_sess_new) */
	{
		/* DiamId is provided, not opt */
		CHECK( 0, fd_sess_new( &sess1, TEST_DIAM_ID, NULL, 0 ) );
		CHECK( 0, fd_sess_new( &sess2, TEST_DIAM_ID, NULL, 0 ) );
		#if 0
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		
		/* Check both string start with the diameter Id, but are different */
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		CHECK( 0, strncmp(str1, TEST_DIAM_ID ";", strlen(TEST_DIAM_ID) + 1) );
		CHECK( 0, fd_sess_getsid(sess2, &str2) );
		CHECK( 0, strncmp(str2, TEST_DIAM_ID ";", strlen(TEST_DIAM_ID) + 1) );
		CHECK( 1, strcmp(str1, str2) ? 1 : 0 );
		CHECK( 0, fd_sess_destroy( &sess1 ) );
		CHECK( 0, fd_sess_destroy( &sess2 ) );
		
		/* diamId and opt */
		CHECK( 0, fd_sess_new( &sess1, TEST_DIAM_ID, TEST_OPT, 0 ) );
		CHECK( 0, fd_sess_new( &sess2, TEST_DIAM_ID, TEST_OPT, strlen(TEST_OPT) - 1 ) );
		#if 0
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		CHECK( 0, strncmp(str1, TEST_DIAM_ID ";", strlen(TEST_DIAM_ID) + 1) );
		CHECK( 0, strcmp(str1 + strlen(str1) - strlen(TEST_OPT) - 1, ";" TEST_OPT) );
		
		CHECK( 0, fd_sess_getsid(sess2, &str2) );
		CHECK( 0, strncmp(str2, TEST_DIAM_ID ";", strlen(TEST_DIAM_ID) + 1) );
		CHECK( 0, strncmp(str2 + strlen(str2) - strlen(TEST_OPT), ";" TEST_OPT, strlen(TEST_OPT)) );
		
		CHECK( 1, strcmp(str1, str2) ? 1 : 0 );
		CHECK( 0, fd_sess_destroy( &sess1 ) );
		CHECK( 0, fd_sess_destroy( &sess2 ) );
		
		/* Now, only opt is provided */
		CHECK( 0, fd_sess_new( &sess1, NULL, TEST_SID, 0 ) );
		CHECK( EALREADY, fd_sess_new( &sess2, NULL, TEST_SID, 0 ) );
		CHECK( sess2, sess1 );
		CHECK( EALREADY, fd_sess_new( &sess3, NULL, TEST_SID, strlen(TEST_SID) ) );
		CHECK( sess3, sess1 );
		CHECK( 0, fd_sess_new( &sess2, NULL, TEST_SID, strlen(TEST_SID) - 1 ) );
		#if 0
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		CHECK( 0, fd_sess_getsid(sess2, &str2) );
		CHECK( 0, strncmp( str1, str2, strlen(TEST_SID) - 1 ) );
		CHECK( 0, strcmp( str1, TEST_SID ) );
		
		CHECK( 0, fd_sess_destroy( &sess2 ) );
		CHECK( 0, fd_sess_destroy( &sess1 ) );
	}
		
	/* Test fd_sess_fromsid */
	{
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess2, &new ) );
		CHECK( 0, new );
		CHECK( sess1, sess2 );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess3, NULL ) );
		CHECK( sess1, sess3 );
		
		CHECK( 0, fd_sess_destroy( &sess1 ) );
	}
	
	/* Test fd_sess_reclaim */
	{
		struct mystate *tms;
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		CHECK( 0, fd_sess_reclaim( &sess1 ) );
		CHECK( NULL, sess1 );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		tms = new_state(TEST_SID, NULL);
		CHECK( 0, fd_sess_state_store ( hdl1, sess1, &tms ) );
		
		CHECK( 0, fd_sess_reclaim( &sess1 ) );
		CHECK( NULL, sess1 );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 0, new );
		
		CHECK( 0, fd_sess_destroy( &sess1 ) );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		CHECK( 0, fd_sess_destroy( &sess1 ) );
	}
	
	/* Test timeout function */
	{
		struct timespec timeout;
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &timeout) );
		CHECK( 0, fd_sess_settimeout( sess1, &timeout) );
		timeout.tv_sec = 0;
		timeout.tv_nsec= 50000000; /* 50 ms */
		CHECK( 0, nanosleep(&timeout, NULL) );
		
		CHECK( 0, fd_sess_fromsid( TEST_SID, strlen(TEST_SID), &sess1, &new ) );
		CHECK( 1, new ? 1 : 0 );
		
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &timeout) );
		timeout.tv_sec += 2678500; /* longer that SESS_DEFAULT_LIFETIME */
		CHECK( 0, fd_sess_settimeout( sess1, &timeout) );
		
		/* Create a second session */
		CHECK( 0, fd_sess_new( &sess2, TEST_DIAM_ID, NULL, 0 ) );
		
		/* We don't really have away to verify the expiry list is in proper order automatically here... */
		
		CHECK( 0, fd_sess_destroy( &sess2 ) );
		CHECK( 0, fd_sess_destroy( &sess1 ) );
	}
	
	
	/* Test states operations */
	{
		struct mystate * ms[6], *tms;
		int freed[6];
		struct timespec timeout;
		
		/* Create three sessions */
		CHECK( 0, fd_sess_new( &sess1, TEST_DIAM_ID, NULL, 0 ) );
		CHECK( 0, fd_sess_new( &sess2, TEST_DIAM_ID, NULL, 0 ) );
		CHECK( 0, fd_sess_new( &sess3, TEST_DIAM_ID, NULL, 0 ) );
		
		/* Create 2 states */
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		freed[0] = 0;
		ms[0] = new_state(str1, &freed[0]);
		ms[1] = new_state(str1, NULL);

		tms = ms[0]; /* save a copy */
		CHECK( 0, fd_sess_state_store ( hdl1, sess1, &ms[0] ) );
		CHECK( NULL, ms[0] );
		CHECK( EINVAL, fd_sess_state_store ( hdl1, sess1, NULL ) );
		CHECK( EALREADY, fd_sess_state_store ( hdl1, sess1, &ms[1] ) );
		CHECK( 1, ms[1] ? 1 : 0 );
		
		#if 0
		fd_sess_dump(0, sess1);
		#endif
		
		CHECK( 0, fd_sess_state_retrieve( hdl1, sess1, &ms[0] ) );
		CHECK( tms, ms[0] );
		CHECK( 0, freed[0] );
		
		CHECK( 0, fd_sess_state_retrieve( hdl1, sess2, &tms ) );
		CHECK( NULL, tms );
		
		mycleanup(ms[0], str1);
		mycleanup(ms[1], str1);
		
		/* Now create 6 states */
		memset(&freed[0], 0, sizeof(freed));
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		ms[0] = new_state(str1, &freed[0]);
		ms[1] = new_state(str1, &freed[1]);
		CHECK( 0, fd_sess_getsid(sess2, &str1) );
		ms[2] = new_state(str1, &freed[2]);
		ms[3] = new_state(str1, &freed[3]);
		CHECK( 0, fd_sess_getsid(sess3, &str1) );
		ms[4] = new_state(str1, &freed[4]);
		ms[5] = new_state(str1, &freed[5]);
		str2 = strdup(str1);
		CHECK( 1, str2 ? 1 : 0 );
		
		/* Store the six states */
		CHECK( 0, fd_sess_state_store ( hdl1, sess1, &ms[0] ) );
		CHECK( 0, fd_sess_state_store ( hdl2, sess1, &ms[1] ) );
		CHECK( 0, fd_sess_state_store ( hdl1, sess2, &ms[2] ) );
		CHECK( 0, fd_sess_state_store ( hdl2, sess2, &ms[3] ) );
		CHECK( 0, fd_sess_state_store ( hdl1, sess3, &ms[4] ) );
		CHECK( 0, fd_sess_state_store ( hdl2, sess3, &ms[5] ) );
		
		#if 0
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		fd_sess_dump(0, sess3);
		#endif
		
		/* Destroy session 3 */
		CHECK( 0, fd_sess_destroy( &sess3 ) );
		CHECK( 0, freed[0] );
		CHECK( 0, freed[1] );
		CHECK( 0, freed[2] );
		CHECK( 0, freed[3] );
		CHECK( 1, freed[4] );
		CHECK( 1, freed[5] );
		
		/* Destroy handler 2 */
		CHECK( 0, fd_sess_handler_destroy( &hdl2 ) );
		CHECK( 0, freed[0] );
		CHECK( 1, freed[1] );
		CHECK( 0, freed[2] );
		CHECK( 1, freed[3] );
		CHECK( 1, freed[4] );
		CHECK( 1, freed[5] );
		
		#if 1
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		
		/* Create again session 3, check that no data is associated to it */
		CHECK( 0, fd_sess_fromsid( str2, strlen(str2), &sess3, &new ) );
		CHECK( 1, new ? 1 : 0 );
		CHECK( 0, fd_sess_state_retrieve( hdl1, sess3, &tms ) );
		CHECK( NULL, tms );
		CHECK( 0, fd_sess_destroy( &sess3 ) );
		free(str2);
		
		/* Timeout does call cleanups */
		CHECK( 0, clock_gettime(CLOCK_REALTIME, &timeout) );
		CHECK( 0, fd_sess_settimeout( sess2, &timeout) );
		#if 1
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		timeout.tv_sec = 0;
		timeout.tv_nsec= 50000000; /* 50 ms */
		CHECK( 0, nanosleep(&timeout, NULL) );
		CHECK( 0, freed[0] );
		CHECK( 1, freed[1] );
		CHECK( 1, freed[2] );
		CHECK( 1, freed[3] );
		CHECK( 1, freed[4] );
		CHECK( 1, freed[5] );
		
		/* Check the last data can still be retrieved */
		CHECK( 0, fd_sess_state_retrieve( hdl1, sess1, &tms ) );
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		mycleanup(tms, str1);
	}
	
	
	/* That's all for the tests yet */
	PASSTEST();
} 
	
