/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2009, WIDE Project and NICT								 *
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

void mycleanup( char * sid, struct mystate * data )
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
		#if 1
		fd_sess_dump_hdl(0, hdl1);
		fd_sess_dump_hdl(0, hdl2);
		#endif
	}
	
	/* Test Session Id generation (fd_sess_new) */
	{
		/* DiamId is provided, not opt */
		CHECK( 0, fd_sess_new( &sess1, TEST_DIAM_ID, NULL, 0 ) );
		CHECK( 0, fd_sess_new( &sess2, TEST_DIAM_ID, NULL, 0 ) );
		#if 1
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
		#if 1
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
		#if 1
		fd_sess_dump(0, sess1);
		fd_sess_dump(0, sess2);
		#endif
		CHECK( 0, fd_sess_getsid(sess1, &str1) );
		CHECK( 0, fd_sess_getsid(sess2, &str2) );
		CHECK( 0, strncmp( str1, str2, strlen(TEST_SID) - 1 ) );
		CHECK( 0, strcmp( str1, TEST_SID ) );
		
		CHECK( 0, fd_sess_destroy( &sess2 ) );
	}
		
		
	
/*	
int fd_sess_new ( struct session ** session, char * diamId, char * opt, size_t optlen );
int fd_sess_fromsid ( char * sid, size_t len, struct session ** session, int * new);
int fd_sess_getsid ( struct session * session, char ** sid );
int fd_sess_settimeout( struct session * session, const struct timespec * timeout );
int fd_sess_destroy ( struct session ** session );
*/

	/* That's all for the tests yet */
	PASSTEST();
} 
	
