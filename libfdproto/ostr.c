/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2011, WIDE Project and NICT								 *
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

/* Similar to strdup with (must be verified) os0_t */
os0_t os0dup_int(os0_t s, size_t l) {
	os0_t r;
	CHECK_MALLOC_DO( r = calloc(l+1, 1), return NULL );
	memcpy(r, s, l); /* this might be faster than a strcpy or strdup because it can work with 32 or 64b blocks */
	return r;
}

/* case sensitive comparison, fast */
int fd_os_cmp_int(uint8_t * os1, size_t os1sz, uint8_t * os2, size_t os2sz)
{
	ASSERT( os1 && os2);
	if (os1sz < os2sz)
		return -1;
	if (os1sz > os2sz)
		return 1;
	return memcmp(os1, os2, os1sz);
}

/* a local version of tolower() that does not depend on LC_CTYPE locale */
static inline uint8_t asciitolower(uint8_t a)
{
	if ((a >= 'A') && (a <= 'Z'))
		return a + 32 /* == 'a' - 'A' */;
	return a;
}

/* a little less sensitive to case, slower. */
int fd_os_almostcasecmp_int(uint8_t * os1, size_t os1sz, uint8_t * os2, size_t os2sz)
{
	int i;
	ASSERT( os1 && os2);
	if (os1sz < os2sz)
		return -1;
	if (os1sz > os2sz)
		return 1;
	
	for (i = 0; i < os1sz; i++) {
		if (os1[i] == os2[i])
			continue;
		
		if (asciitolower(os1[i]) == asciitolower(os2[i])) 
			continue;
		
		return os1[i] < os2[i] ? -1 : 1;
	}
	
	return 0;
}

/* Check if the string contains only ASCII */
int fd_os_is_valid_DiameterIdentity(uint8_t * os, size_t ossz)
{
	int i;
	
	/* Allow letters, digits, hyphen, dot */
	for (i=0; i < ossz; i++) {
		if (os[i] > 'z')
			break;
		if (os[i] >= 'a')
			continue;
		if ((os[i] >= 'A') && (os[i] <= 'Z'))
			continue;
		if ((os[i] == '-') || (os[i] == '.'))
			continue;
		if ((os[i] >= '0') && (os[i] <= '9'))
			continue;
		break;
	}
	if (i < ossz) {
		TRACE_DEBUG(INFO, "Invalid character '%c' in DiameterIdentity '%.*s'", os[i], ossz, os);
		return 0;
	}
	return 1;
}

/* The following function validates a string as a Diameter Identity or applies the IDNA transformation on it */
int fd_os_validate_DiameterIdentity(char ** id, size_t * outsz, int memory /* 0: *id can be realloc'd. 1: *id must be malloc'd on output (was static) */)
{
	TRACE_ENTRY("%p %p", id, outsz);
	
	*outsz = strlen(*id);
	
	if (!fd_os_is_valid_DiameterIdentity((os0_t)*id, *outsz)) {
		char buf[HOST_NAME_MAX];
		
		TODO("Stringprep in into buf");
		TRACE_DEBUG(INFO, "The string '%s' is not a valid DiameterIdentity, it was changed to '%s'", *id, buf);
		TODO("Realloc *id if !memory");
		/* copy buf */
		/* update the size */
		return ENOTSUP;
	} else {
		if (memory == 1) {
			CHECK_MALLOC( *id = os0dup(*id, *outsz) );
		}
	}
	return 0;
}





/********************************************************************************************************/
/* Hash function -- credits to Austin Appleby, thank you ^^ */
/* See http://murmurhash.googlepages.com for more information on this function */

/* the strings are NOT always aligned properly (ex: received in RADIUS message), so we use the aligned MurmurHash2 function as needed */
#define _HASH_MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
uint32_t fd_os_hash ( uint8_t * string, size_t len )
{
	uint32_t hash = len;
	uint8_t * data = string;
	
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	int align = (long)string & 3;
	
	if (!align || (len < 4)) {
		/* In case data is aligned, MurmurHash2 function */
		while(len >= 4)
		{
			/* Mix 4 bytes at a time into the hash */
			uint32_t k = *(uint32_t *)data;	/* We don't care about the byte order */

			_HASH_MIX(hash, k, m);

			data += 4;
			len -= 4;
		}

		/* Handle the last few bytes of the input */
		switch(len) {
			case 3: hash ^= data[2] << 16;
			case 2: hash ^= data[1] << 8;
			case 1: hash ^= data[0];
	        		hash *= m;
		}
		
	} else {
		/* Unaligned data, use alignment-safe slower version */
		
		/* Pre-load the temp registers */
		uint32_t t = 0, d = 0;
		switch(align)
		{
			case 1: t |= data[2] << 16;
			case 2: t |= data[1] << 8;
			case 3: t |= data[0];
		}
		t <<= (8 * align);

		data += 4-align;
		len -= 4-align;
		
		/* From this point, "data" can be read by chunks of 4 bytes */
		
		int sl = 8 * (4-align);
		int sr = 8 * align;

		/* Mix */
		while(len >= 4)
		{
			uint32_t k;
			
			d = *(unsigned int *)data;
			k = (t >> sr) | (d << sl);

			_HASH_MIX(hash, k, m);

			t = d;

			data += 4;
			len -= 4;
		}

		/* Handle leftover data in temp registers */
		d = 0;
		if(len >= align)
		{
			uint32_t k;
			
			switch(align)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			}

			k = (t >> sr) | (d << sl);
			_HASH_MIX(hash, k, m);

			data += align;
			len -= align;

			/* Handle tail bytes */

			switch(len)
			{
			case 3: hash ^= data[2] << 16;
			case 2: hash ^= data[1] << 8;
			case 1: hash ^= data[0];
					hash *= m;
			};
		}
		else
		{
			switch(len)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			case 0: hash ^= (t >> sr) | (d << sl);
					hash *= m;
			}
		}


	}

	/* Do a few final mixes of the hash to ensure the last few
	   bytes are well-incorporated. */
	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;

	return hash;
} 

