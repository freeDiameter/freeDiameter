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

#include "libfD.h"

/* Initialize a list element */
void fd_list_init ( struct fd_list * list, void * obj )
{
	memset(list, 0, sizeof(struct fd_list));
	list->next = list;
	list->prev = list;
	list->head = list;
	list->o    = obj;
}

#define CHECK_SINGLE( li ) {			\
	ASSERT( ((struct fd_list *)(li))->next == (li) );	\
	ASSERT( ((struct fd_list *)(li))->prev == (li) );	\
	ASSERT( ((struct fd_list *)(li))->head == (li) );	\
}

/* insert after a reference, checks done */
static void list_insert_after( struct fd_list * ref, struct fd_list * item )
{
	item->prev = ref;
	item->next = ref->next;
	item->head = ref->head;
	ref->next->prev = item;
	ref->next = item;
}

/* insert after a reference */
void fd_list_insert_after  ( struct fd_list * ref, struct fd_list * item )
{
	ASSERT(item != NULL);
	ASSERT(ref != NULL);
	CHECK_SINGLE ( item );
	ASSERT(ref->head != item);
	list_insert_after(ref, item);
}

/* Move all elements of list senti at the end of list ref */
void fd_list_move_end(struct fd_list * ref, struct fd_list * senti)
{
	ASSERT(ref->head == ref);
	ASSERT(senti->head == senti);
	
	if (senti->next == senti)
		return;
	
	senti->next->prev = ref->prev;
	ref->prev->next   = senti->next;
	senti->prev->next = ref;
	ref->prev         = senti->prev;
	senti->prev = senti;
	senti->next = senti;
}

/* insert before a reference, checks done */
static void list_insert_before ( struct fd_list * ref, struct fd_list * item )
{
	item->prev = ref->prev;
	item->next = ref;
	item->head = ref->head;
	ref->prev->next = item;
	ref->prev = item;
}

/* insert before a reference */
void fd_list_insert_before ( struct fd_list * ref, struct fd_list * item )
{
	ASSERT(item != NULL);
	ASSERT(ref != NULL);
	CHECK_SINGLE ( item );
	ASSERT(ref->head != item);
	list_insert_before(ref, item);
}

/* Insert an item in an ordered list -- ordering function provided. If duplicate object found, it is returned in ref_duplicate */
int fd_list_insert_ordered( struct fd_list * head, struct fd_list * item, int (*cmp_fct)(void *, void *), void ** ref_duplicate)
{
	struct fd_list * ptr = head;
	int cmp;
	
	/* Some debug sanity checks */
	ASSERT(head != NULL);
	ASSERT(item != NULL);
	ASSERT(cmp_fct != NULL);
	ASSERT(head->head == head);
	CHECK_SINGLE ( item );
	
	/* loop in the list */
	while (ptr->next != head)
	{
		/* Compare the object to insert with the next object in list */
		cmp = cmp_fct( item->o, ptr->next->o );
		if (!cmp) {
			/* An element with the same key already exists */
			if (ref_duplicate != NULL)
				*ref_duplicate = ptr->next->o;
			return EEXIST;
		}
		
		if (cmp < 0)
			break; /* We must insert the element here */
		
		ptr = ptr->next;
	}
	
	/* Now insert the element between ptr and ptr->next */
	list_insert_after( ptr, item );
	
	/* Ok */
	return 0;
}
	
/* Unlink an object */
void fd_list_unlink ( struct fd_list * item )
{
	ASSERT(item != NULL);
	if (item->head == item)
		return;
	/* unlink */
	item->next->prev = item->prev;
	item->prev->next = item->next;
	/* sanitize */
	item->next = item;
	item->prev = item;
	item->head = item;
}


/********************************************************************************************************/
/* Hash function -- credits to Austin Appleby, thank you ^^ */
/* See http://murmurhash.googlepages.com for more information on this function */

/* the strings are NOT always aligned properly (ex: received in RADIUS message), so we use the aligned MurmurHash2 function as needed */
#define _HASH_MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
uint32_t fd_hash ( char * string, size_t len )
{
	uint32_t hash = len;
	char * data = string;
	
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
