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

#include "fD.h"

#include <dlfcn.h>	/* We may use libtool's <ltdl.h> later for better portability.... */

/* plugins management */

/* List of extensions to load, from the configuration parsing */
struct fd_ext_info {
	struct fd_list	chain;		/* link in the list */
	char 		*filename;	/* extension filename. must be a dynamic library with fd_ext_init symbol. */
	char 		*conffile;	/* optional configuration file name for the extension */
	void 		*handler;	/* object returned by dlopen() */
	void		(*fini)(void);	/* optional address of the fd_ext_fini callback */
};

/* list of extensions */
static struct fd_list ext_list;

/* Initialize the module */
int fd_ext_init()
{
	TRACE_ENTRY();
	fd_list_init(&ext_list, NULL);
	return 0;
}

/* Add new extension */
int fd_ext_add( char * filename, char * conffile )
{
	struct fd_ext_info * new;
	
	TRACE_ENTRY("%p(%s) %p(%s)", filename, filename?filename:"", conffile, conffile?conffile:"");
	
	/* Check the filename is valid */
	CHECK_PARAMS( filename );
	
	/* Create a new object in the list */
	CHECK_MALLOC(  new = malloc( sizeof(struct fd_ext_info) )  );
	memset(new, 0, sizeof(struct fd_ext_info));
	fd_list_init(&new->chain, NULL);
	new->filename = filename;
	new->conffile = conffile;
	fd_list_insert_before( &ext_list, &new->chain );
	TRACE_DEBUG (FULL, "Extension %s added to the list.", filename);
	return 0;
}

/* Load all extensions in the list */
int fd_ext_load()
{
	int ret;
	int (*fd_ext_init)(int, int, char *) = NULL;
	struct fd_list * li;
	
	TRACE_ENTRY();
	
	/* Loop on all extensions */
	for (li = ext_list.next; li != &ext_list; li = li->next)
	{
		struct fd_ext_info * ext = (struct fd_ext_info *)li;
		TRACE_DEBUG (INFO, "Loading : %s", ext->filename);
		
		/* Load the extension */
#ifndef DEBUG
		ext->handler = dlopen(ext->filename, RTLD_LAZY | RTLD_GLOBAL);
#else /* DEBUG */
		/* We resolve immediatly so it's easier to find problems in ABI */
		ext->handler = dlopen(ext->filename, RTLD_NOW | RTLD_GLOBAL);
#endif /* DEBUG */
		if (ext->handler == NULL) {
			/* An error occured */
			TRACE_DEBUG( NONE, "Loading of extension %s failed:\n %s\n", ext->filename, dlerror());
			return EINVAL;
		}
		
		/* Resolve the entry point of the extension */
		fd_ext_init = ( int (*) (int, int, char *) )dlsym( ext->handler, "fd_ext_init" );
		
		if (fd_ext_init == NULL) {
			/* An error occured */
			TRACE_DEBUG( NONE, "Unable to resolve symbol 'fd_ext_init' for extension %s:\n %s\n", ext->filename, dlerror());
			return EINVAL;
		}
		
		/* Resolve the exit point of the extension, which is optional for extensions */
		ext->fini = ( void (*) (void) )dlsym( ext->handler, "fd_ext_fini" );
		
		if (ext->fini == NULL) {
			/* Not provided */
			TRACE_DEBUG (FULL, "Extension [%s] has no fd_ext_fini function.", ext->filename);
		} else {
			/* Provided */
			TRACE_DEBUG (FULL, "Extension [%s] fd_ext_fini has been resolved successfully.", ext->filename);
		}
		
		/* Now call the entry point to initialize the extension */
		ret = (*fd_ext_init)( FD_PROJECT_VERSION_MAJOR, FD_PROJECT_VERSION_MINOR, ext->conffile );
		if (ret != 0) {
			/* The extension was unable to load cleanly */
			TRACE_DEBUG( NONE, "Extension %s returned an error during initialization: %s\n", ext->filename, strerror(ret));
			return ret;
		}
		
		/* Proceed to the next extension */
	}

	TRACE_DEBUG (INFO, "All extensions loaded.");
	
	/* We have finished. */
	return 0;
}

/* Now unload the extensions and free the memory */
int fd_ext_fini( void ) 
{
	TRACE_ENTRY();
	
	/* Loop on all extensions, in FIFO order */
	while (!FD_IS_LIST_EMPTY(&ext_list))
	{
		struct fd_list * li = ext_list.next;
		struct fd_ext_info * ext = (struct fd_ext_info *)li;
	
		/* Unlink this element from the list */
		fd_list_unlink(li);
		
		/* Call the exit point of the extension, if it was resolved */
		if (ext->fini != NULL) {
			TRACE_DEBUG (FULL, "Calling [%s]->fd_ext_fini function.", ext->filename);
			(*ext->fini)();
		}
		
		/* Now unload the extension */
		if (ext->handler) {
			TRACE_DEBUG (FULL, "Unloading %s", ext->filename);
			if ( dlclose(ext->handler) != 0 ) {
				TRACE_DEBUG (INFO, "Unloading [%s] failed : %s\n", ext->filename, dlerror());
			}
		}
		
		/* Free the object and continue */
		free(ext->filename);
		free(ext->conffile);
		free(ext);
	}
	
	/* We always return 0 since we would not handle an error anyway... */
	return 0;
}

