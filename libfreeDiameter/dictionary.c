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

/* Names of the base types */
const char * type_base_name[] = { /* must keep in sync with dict_avp_basetype */
	"GROUPED", 	/* AVP_TYPE_GROUPED */
	"OCTETSTRING", 	/* AVP_TYPE_OCTETSTRING */
	"INTEGER32", 	/* AVP_TYPE_INTEGER32 */
	"INTEGER64", 	/* AVP_TYPE_INTEGER64 */
	"UNSIGNED32", 	/* AVP_TYPE_UNSIGNED32 */
	"UNSIGNED64", 	/* AVP_TYPE_UNSIGNED64 */
	"FLOAT32", 	/* AVP_TYPE_FLOAT32 */
	"FLOAT64"	/* AVP_TYPE_FLOAT64 */
	};

/* The number of lists in an object */
#define NB_LISTS_PER_OBJ	3

/* Some eye catchers definitions */
#define OBJECT_EYECATCHER	(0x0b13c7)
#define DICT_EYECATCHER		(0x00d1c7)

/* Definition of the dictionary objects */
struct dict_object {
	enum dict_object_type	type;	/* What type of object is this? */
	int			objeyec;/* eyecatcher for this object */
	int			typeyec;/* eyecatcher for this type of object */
	struct dictionary	*dico;  /* The dictionary this object belongs to */
	
	union {
		struct dict_vendor_data		vendor;
		struct dict_application_data	application;
		struct dict_type_data		type;
		struct dict_enumval_data	enumval;
		struct dict_avp_data		avp;
		struct dict_cmd_data		cmd;
		struct dict_rule_data		rule;
	} data;				/* The data of this object */
	
	struct dict_object *	parent; /* The parent of this object, if any */
	
	struct fd_list		list[NB_LISTS_PER_OBJ];/* used to chain objects.*/
	/* More information about the lists :
	
	 - the use for each list depends on the type of object. See detail bellow.
	 
	 - a sentinel for a list has its 'o' field cleared. (this is the criteria to detect end of a loop)
	 
	 - The lists are always ordered. The criteria are described bellow. the functions to order them are referenced in dict_obj_info
	 
	 - The dict_lock must be held for any list operation.
	 
	 => VENDORS:
	 list[0]: list of the vendors, ordered by their id. The sentinel is g_dict_vendors (vendor with id 0)
	 list[1]: sentinel for the list of AVPs from this vendor, ordered by AVP code.
	 list[2]: sentinel for the list of AVPs from this vendor, ordered by AVP name.
	 
	 => APPLICATIONS:
	 list[0]: list of the applications, ordered by their id. The sentinel is g_dict_applications (application with id 0)
	 list[1]: not used
	 list[2]: not used.
	 
	 => TYPES:
	 list[0]: list of the types, ordered by their names. The sentinel is g_list_types.
	 list[1]: sentinel for the type_enum list of this type, ordered by their constant name.
	 list[2]: sentinel for the type_enum list of this type, ordered by their constant value.
	 
	 => TYPE_ENUMS:
	 list[0]: list of the contants for a given type, ordered by the constant name. Sentinel is a (list[1]) element of a TYPE object.
	 list[1]: list of the contants for a given type, ordered by the constant value. Sentinel is a (list[2]) element of a TYPE object.
	 list[2]: not used
	 
	 => AVPS:
	 list[0]: list of the AVP from a given vendor, ordered by avp code. Sentinel is a list[1] element of a VENDOR object.
	 list[1]: list of the AVP from a given vendor, ordered by avp name. Sentinel is a list[2] element of a VENDOR object.
	 list[2]: sentinel for the rule list that apply to this AVP.
	 
	 => COMMANDS:
	 list[0]: list of the commands, ordered by their names. The sentinel is g_list_cmd_name.
	 list[1]: list of the commands, ordered by their command code and 'R' flag. The sentinel is g_list_cmd_code.
	 list[2]: sentinel for the rule list that apply to this command.
	 
	 => RULES:
	 list[0]: list of the rules for a given (grouped) AVP or Command, ordered by the AVP name to which they refer. sentinel is list[2] of a command or (grouped) avp.
	 list[1]: not used
	 list[2]: not used.
	 
	 */
	 
	 /* Sentinel for the dispatch callbacks */
	 struct fd_list		disp_cbs;
	
};

/* Definition of the dictionary structure */
struct dictionary {
	int		 	dict_eyec;		/* Eye-catcher for the dictionary (DICT_EYECATCHER) */
	
	pthread_rwlock_t 	dict_lock;		/* The global rwlock for the dictionary */
	
	struct dict_object	dict_vendors;		/* Sentinel for the list of vendors, corresponding to vendor 0 */
	struct dict_object	dict_applications;	/* Sentinel for the list of applications, corresponding to app 0 */
	struct fd_list		dict_types;		/* Sentinel for the list of types */
	struct fd_list		dict_cmd_name;		/* Sentinel for the list of commands, ordered by names */
	struct fd_list		dict_cmd_code;		/* Sentinel for the list of commands, ordered by codes */
	
	struct dict_object	dict_cmd_error;		/* Special command object for answers with the 'E' bit set */
	
	int			dict_count[DICT_TYPE_MAX]; /* Number of objects of each type */
};

/* Forward declarations of dump functions */
static void dump_vendor_data 	  ( void * data );
static void dump_application_data ( void * data );
static void dump_type_data 	  ( void * data );
  /* the dump function for enum has a different prototype since it need the datatype */
static void dump_avp_data 	  ( void * data );
static void dump_command_data 	  ( void * data );
static void dump_rule_data 	  ( void * data );

/* Forward declarations of search functions */
static int search_vendor 	( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_application   ( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_type 		( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_enumval 	( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_avp		( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_cmd		( struct dictionary * dict, int criteria, void * what, struct dict_object **result );
static int search_rule		( struct dictionary * dict, int criteria, void * what, struct dict_object **result );

/* The following array contains lot of data about the different types of objects, for automated handling */
static struct {
	enum dict_object_type 	type; 		/* information for this type */
	char *			name;		/* string describing this object, for debug */
	size_t			datasize;	/* The size of the data structure */
	int			parent;		/* 0: never; 1: may; 2: must */
	enum dict_object_type	parenttype;	/* The type of the parent, when relevant */
	int			eyecatcher;	/* A kind of signature for this object */
	void 		      (*dump_data)(void * data );	/* The function to dump the data section */
	int 		      (*search_fct)(struct dictionary * dict, int criteria, void * what, struct dict_object **result );;	/* The function to search an object of this type */
	int			haslist[NB_LISTS_PER_OBJ];	/* Tell if this list is used */
} dict_obj_info[] = { { 0, "(error)", 0, 0, 0, 0, NULL, NULL, {0, 0, 0} }

	/* type			 name		datasize		   	  parent  	parenttype 
			eyecatcher		dump_data	  	search_fct,		haslist[] 	*/

	,{ DICT_VENDOR,		"VENDOR",	sizeof(struct dict_vendor_data),	0, 	0,
			OBJECT_EYECATCHER + 1, 	dump_vendor_data, 	search_vendor, 		{ 1, 0, 0 } }
	
	,{ DICT_APPLICATION,	"APPLICATION",	sizeof(struct dict_application_data),	1, 	DICT_VENDOR,
			OBJECT_EYECATCHER + 2,	dump_application_data,	search_application,	{ 1, 0, 0 } }
	
	,{ DICT_TYPE,		"TYPE",		sizeof(struct dict_type_data),		1, 	DICT_APPLICATION,
			OBJECT_EYECATCHER + 3,	dump_type_data,		search_type,		{ 1, 0, 0 } }
	
	,{ DICT_ENUMVAL,	"ENUMVAL",	sizeof(struct dict_enumval_data),	2, 	DICT_TYPE,
			OBJECT_EYECATCHER + 4,	NULL,			search_enumval,	{ 1, 1, 0 } }
	
	,{ DICT_AVP,		"AVP",		sizeof(struct dict_avp_data),		1, 	DICT_TYPE,
			OBJECT_EYECATCHER + 5,	dump_avp_data,		search_avp,		{ 1, 1,	0 } }
	
	,{ DICT_COMMAND,	"COMMAND",	sizeof(struct dict_cmd_data),		1, 	DICT_APPLICATION,
			OBJECT_EYECATCHER + 6,	dump_command_data,	search_cmd,		{ 1, 1, 0 } }
	
	,{ DICT_RULE,		"RULE",		sizeof(struct dict_rule_data),		2, 	-1 /* special case: grouped avp or command */,
			OBJECT_EYECATCHER + 7,	dump_rule_data,		search_rule,		{ 1, 0, 0 } }
	
};
	
/* Macro to verify a "type" value */
#define CHECK_TYPE( type ) ( ((type) > 0) && ((type) <= DICT_TYPE_MAX) )

/* Cast macro */
#define _O( object ) ((struct dict_object *) (object))

/* Get information line for a given object */
#define _OBINFO(object) (dict_obj_info[CHECK_TYPE(_O(object)->type) ? _O(object)->type : 0])




/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Objects management                                                 */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* Functions to manage the objects creation and destruction. */

/* Duplicate a string inplace */
#define DUP_string( str ) {				\
	char * __str = (str);				\
	CHECK_MALLOC( (str) = strdup(__str) );		\
}
	
/* Initialize an object */
static void init_object( struct dict_object * obj, enum dict_object_type type )
{
	int i;
	
	TRACE_ENTRY("%p %d", obj, type);
	
	/* Clean the object first */
	memset ( obj, 0, sizeof(struct dict_object));
	
	CHECK_PARAMS_DO(  CHECK_TYPE(type),  return  );

	obj->type = type;
	obj->objeyec = OBJECT_EYECATCHER;
	obj->typeyec = _OBINFO(obj).eyecatcher;

	/* We don't initialize the data nor the parent here */
	
	/* Now init the lists */
	for (i=0; i<NB_LISTS_PER_OBJ; i++) {
		if (_OBINFO(obj).haslist[i] != 0) 
			fd_list_init(&obj->list[i], obj);
		else
			fd_list_init(&obj->list[i], NULL);
	}
	
	fd_list_init(&obj->disp_cbs, NULL);
}

/* Initialize the "data" part of an object */
static int init_object_data(void * dest, void * source, enum dict_object_type type)
{
	TRACE_ENTRY("%p %p %d", dest, source, type);
	CHECK_PARAMS( dest && source && CHECK_TYPE(type) );
	
	/* Generic: copy the full data structure */	
	memcpy( dest, source, dict_obj_info[type].datasize );
	
	/* Then strings must be duplicated, not copied */
	/* This function might be simplified by always defining the "name" field as the first field of the structures, but... it's error-prone */
	switch (type) {
		case DICT_VENDOR:
			DUP_string( ((struct dict_vendor_data *)dest)->vendor_name );
			break;
		
		case DICT_APPLICATION:
			DUP_string( ((struct dict_application_data *)dest)->application_name );
			break;
			
		case DICT_TYPE:
			DUP_string( ((struct dict_type_data *)dest)->type_name );
			break;
			
		case DICT_ENUMVAL:
			DUP_string( ((struct dict_enumval_data *)dest)->enum_name );
			break;

		case DICT_AVP:
			DUP_string( ((struct dict_avp_data *)dest)->avp_name );
			break;
			
		case DICT_COMMAND:
			DUP_string( ((struct dict_cmd_data *)dest)->cmd_name );
			break;
		
		default:
			/* Nothing to do for RULES */
			;
	}
	
	return 0;
}

/* Check that an object is valid (1: OK, 0: error) */
static int verify_object( struct dict_object * obj )
{
	TRACE_ENTRY("%p", obj);
	
	CHECK_PARAMS_DO(  obj
			&& (obj->objeyec == OBJECT_EYECATCHER)
			&& CHECK_TYPE(obj->type)
			&& (obj->typeyec == dict_obj_info[obj->type].eyecatcher),
		{
			if (obj) {
				TRACE_DEBUG(FULL, "Invalid object : %p\n"
						  "     obj->objeyec : %x / %x\n"
						  "     obj->type    : %d\n"
						  "     obj->objeyec : %x / %x\n"
						  "     obj->typeyec : %x / %x", 
						obj,
						obj->objeyec, OBJECT_EYECATCHER,
						obj->type,
						obj->objeyec, OBJECT_EYECATCHER,
						obj->typeyec, _OBINFO(obj).eyecatcher);
			}
			return 0;
		}  );
	
	/* The object is probably valid. */
	return 1;
}

/* Free the data associated to an object */
static void destroy_object_data(struct dict_object * obj)
{
	/* TRACE_ENTRY("%p", obj); */
	
	switch (obj->type) {
		case DICT_VENDOR:
			free( obj->data.vendor.vendor_name );
			break;
		
		case DICT_APPLICATION:
			free( obj->data.application.application_name );
			break;
			
		case DICT_TYPE:
			free( obj->data.type.type_name );
			break;
			
		case DICT_ENUMVAL:
			free( obj->data.enumval.enum_name );
			break;

		case DICT_AVP:
			free( obj->data.avp.avp_name );
			break;
			
		case DICT_COMMAND:
			free( obj->data.cmd.cmd_name );
			break;
		
		default:
			/* nothing to do */
			;
	}
}

/* Forward declaration */
static void destroy_object(struct dict_object * obj);

/* Destroy all objects in a list - the lock must be held */
static void destroy_list(struct fd_list * head) 
{
	/* TRACE_ENTRY("%p", head); */
	
	/* loop in the list */
	while (!FD_IS_LIST_EMPTY(head))
	{
		/* When destroying the object, it is unlinked from the list */
		destroy_object(_O(head->next->o));
	}
}
	
/* Free an object and its sublists */
static void destroy_object(struct dict_object * obj)
{
	int i;
	
	/* TRACE_ENTRY("%p", obj); */
	
	/* Update global count */
	if (obj->dico) 
		obj->dico->dict_count[obj->type]--;
	
	/* Mark the object as invalid */
	obj->objeyec = 0xdead;
	
	/* First, destroy the data associated to the object */
	destroy_object_data(obj);
	
	for (i=0; i<NB_LISTS_PER_OBJ; i++) {
		if (_OBINFO(obj).haslist[i])
			/* unlink the element from the list */
			fd_list_unlink( &obj->list[i] );
		else
			/* This is either a sentinel or unused (=emtpy) list, let's destroy it */
			destroy_list( &obj->list[i] );
	}
	
	/* Unlink all elements from the dispatch list; they will be freed when callback is unregistered */
	while (!FD_IS_LIST_EMPTY(&obj->disp_cbs)) {
		fd_list_unlink( obj->disp_cbs.next );
	}
	
	/* Last, destroy the object */
	free(obj);
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Compare functions                                                  */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* Compare two values */
#define ORDER_scalar( i1, i2 ) \
	((i1 < i2 ) ? -1 : ( i1 > i2 ? 1 : 0 )) 


/* Compare two vendor objects by their id (checks already performed) */
static int order_vendor_by_id ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return ORDER_scalar( o1->data.vendor.vendor_id, o2->data.vendor.vendor_id );
}

/* Compare two application objects by their id (checks already performed) */
static int order_appli_by_id  ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return ORDER_scalar( o1->data.application.application_id, o2->data.application.application_id );
}

/* Compare two type objects by their name (checks already performed) */
static int order_type_by_name ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return strcmp( o1->data.type.type_name, o2->data.type.type_name );
}

/* Compare two type_enum objects by their names (checks already performed) */
static int order_enum_by_name ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return strcmp( o1->data.enumval.enum_name, o2->data.enumval.enum_name );
}

/* Compare two type_enum objects by their values (checks already performed) */
static int order_enum_by_val  ( struct dict_object *o1, struct dict_object *o2 )
{
	size_t oslen;
	int cmp = 0;
	
	TRACE_ENTRY("%p %p", o1, o2);
	
	/* The comparison function depends on the type of data */
	switch ( o1->parent->data.type.type_base ) {
		case AVP_TYPE_OCTETSTRING:
			oslen = o1->data.enumval.enum_value.os.len;
			if (o2->data.enumval.enum_value.os.len < oslen)
				oslen = o2->data.enumval.enum_value.os.len;
			cmp = memcmp(o1->data.enumval.enum_value.os.data, o2->data.enumval.enum_value.os.data, oslen );
			return (cmp ? cmp : ORDER_scalar(o1->data.enumval.enum_value.os.len,o2->data.enumval.enum_value.os.len));
		
		case AVP_TYPE_INTEGER32:
			return ORDER_scalar( o1->data.enumval.enum_value.i32, o2->data.enumval.enum_value.i32 );

		case AVP_TYPE_INTEGER64:
			return ORDER_scalar( o1->data.enumval.enum_value.i64, o2->data.enumval.enum_value.i64 );

		case AVP_TYPE_UNSIGNED32:
			return ORDER_scalar( o1->data.enumval.enum_value.u32, o2->data.enumval.enum_value.u32 );

		case AVP_TYPE_UNSIGNED64:
			return ORDER_scalar( o1->data.enumval.enum_value.u64, o2->data.enumval.enum_value.u64 );

		case AVP_TYPE_FLOAT32:
			return ORDER_scalar( o1->data.enumval.enum_value.f32, o2->data.enumval.enum_value.f32 );

		case AVP_TYPE_FLOAT64:
			return ORDER_scalar( o1->data.enumval.enum_value.f64, o2->data.enumval.enum_value.f64 );

		case AVP_TYPE_GROUPED:
		default:
			ASSERT(0);
	}
	return 0;
}

/* Compare two avp objects by their codes (checks already performed) */
static int order_avp_by_code  ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return ORDER_scalar( o1->data.avp.avp_code, o2->data.avp.avp_code );
}

/* Compare two avp objects by their names (checks already performed) */
static int order_avp_by_name  ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return strcmp( o1->data.avp.avp_name, o2->data.avp.avp_name );
}

/* Compare two command objects by their names (checks already performed) */
static int order_cmd_by_name  ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return strcmp( o1->data.cmd.cmd_name, o2->data.cmd.cmd_name );
}

/* Compare two command objects by their codes and flags (request or answer) (checks already performed) */
static int order_cmd_by_codefl( struct dict_object *o1, struct dict_object *o2 )
{
	uint8_t fl1, fl2;
	int cmp = 0;
	
	TRACE_ENTRY("%p %p", o1, o2);
	
	cmp = ORDER_scalar( o1->data.cmd.cmd_code, o2->data.cmd.cmd_code );
	if (cmp) 
		return cmp;
	
	/* Same command code, we must compare the value of the 'R' flag */
	fl1 = o1->data.cmd.cmd_flag_val & CMD_FLAG_REQUEST;
	fl2 = o2->data.cmd.cmd_flag_val & CMD_FLAG_REQUEST;
	
	/* We want requests first, so we reverse the operators here */
	return ORDER_scalar(fl2, fl1);
		
}

/* Compare two rule object by the AVP name that they refer (checks already performed) */
static int order_rule_by_avpn ( struct dict_object *o1, struct dict_object *o2 )
{
	TRACE_ENTRY("%p %p", o1, o2);
	
	return strcmp( o1->data.rule.rule_avp->data.avp.avp_name, o2->data.rule.rule_avp->data.avp.avp_name );
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Search  functions                                                  */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* Functions used to search for objects in the lists, according to some criteria */

/* On a general note, if result is not NULL, ENOENT is not returned but *result is NULL. */

/* The following macros assume that "what", "ret", "result" (variables), and "end" (label) exist
in the local context where they are called. They are meant to be called only from the functions that follow. */

/* For searchs of type "xxx_OF_xxx": children's parent or default parent */
#define SEARCH_childs_parent( type_of_child, default_parent ) {			\
	struct dict_object *__child = (struct dict_object *) what;		\
	CHECK_PARAMS_DO( verify_object(__child) && 				\
		(__child->type == (type_of_child)), 				\
		   {  ret = EINVAL; goto end;  }  );				\
	ret = 0;								\
	if (result)								\
		*result = (__child->parent ? __child->parent :(default_parent));\
}

/* For search of strings in lists. isindex= 1 if the string is the ordering key of the list */
#define SEARCH_string( str, sentinel, datafield, isindex ) {			\
	char * __str = (char *) str;						\
	int __cmp;								\
	struct fd_list * __li;							\
	ret = 0;								\
	for  (__li = (sentinel); __li->next != (sentinel); __li = __li->next) {	\
		__cmp = strcmp(__str, _O(__li->next->o)->data. datafield );	\
		if (__cmp == 0) {						\
			if (result)						\
				*result = _O(__li->next->o);			\
			goto end;						\
		}								\
		if ((isindex) && (__cmp < 0))					\
			break;							\
	}									\
	if (result)								\
		*result = NULL;							\
	else									\
		ret = ENOENT;							\
}

/* For search of octetstrings in lists (not \0 terminated). */
#define SEARCH_ocstring( ostr, length, sentinel, osdatafield, isindex ) {	\
	unsigned char * __ostr = (unsigned char *) ostr;			\
	int __cmp;								\
	size_t __len;								\
	struct fd_list * __li;							\
	ret = 0;								\
	for  (__li = (sentinel); __li->next != (sentinel); __li = __li->next) {	\
		__len = _O(__li->next->o)->data. osdatafield .len;		\
		if ( __len > (length) )						\
			__len = (length);					\
		__cmp = memcmp(__ostr, 						\
				_O(__li->next->o)->data. osdatafield .data, 	\
				__len);						\
		if (! __cmp) {							\
			__cmp = ORDER_scalar( length,				\
				_O(__li->next->o)->data. osdatafield .len); 	\
		}								\
		if (__cmp == 0) {						\
			if (result)						\
				*result = _O(__li->next->o);			\
			goto end;						\
		}								\
		if ((isindex) && (__cmp < 0))					\
			break;							\
	}									\
	if (result)								\
		*result = NULL;							\
	else									\
		ret = ENOENT;							\
}

/* For search of AVP name in rule lists. */
#define SEARCH_ruleavpname( str, sentinel ) {					\
	char * __str = (char *) str;						\
	int __cmp;								\
	struct fd_list * __li;							\
	ret = 0;								\
	for  (__li = (sentinel); __li->next != (sentinel); __li = __li->next) {	\
		__cmp = strcmp(__str, 						\
		  _O(__li->next->o)->data.rule.rule_avp->data.avp.avp_name);\
		if (__cmp == 0) {						\
			if (result)						\
				*result = _O(__li->next->o);			\
			goto end;						\
		}								\
		if (__cmp < 0)							\
			break;							\
	}									\
	if (result)								\
		*result = NULL;							\
	else									\
		ret = ENOENT;							\
}

/* For search of scalars in lists. isindex= 1 if the value is the ordering key of the list */
#define SEARCH_scalar( value, sentinel, datafield, isindex, defaultobj ) {	\
	int __cmp;								\
	struct fd_list * __li;							\
	ret = 0;								\
	if (  ((defaultobj) != NULL) 						\
		   && (_O(defaultobj)->data. datafield  == value)) {		\
		if (result)							\
			*result = _O(defaultobj);				\
		goto end;							\
	}									\
	for  (__li = (sentinel); __li->next != (sentinel); __li = __li->next) {	\
		__cmp= ORDER_scalar(value, _O(__li->next->o)->data. datafield );\
		if (__cmp == 0) {						\
			if (result)						\
				*result = _O(__li->next->o);			\
			goto end;						\
		}								\
		if ((isindex) && (__cmp < 0))					\
			break;							\
	}									\
	if (result)								\
		*result = NULL;							\
	else									\
		ret = ENOENT;							\
}

/* For search of commands in lists by code and flag. R_flag_val = 0 or CMD_FLAG_REQUEST */
#define SEARCH_codefl( value, R_flag_val, sentinel) {					\
	int __cmp;								\
	struct fd_list * __li;							\
	ret = 0;								\
	for  (	  __li = (sentinel);	 					\
		  __li->next != (sentinel); 					\
		  __li = __li->next) {						\
		__cmp = ORDER_scalar(value, 					\
				_O(__li->next->o)->data.cmd.cmd_code );		\
		if (__cmp == 0) {						\
			uint8_t __mask, __val;					\
			__mask = _O(__li->next->o)->data.cmd.cmd_flag_mask;	\
			__val  = _O(__li->next->o)->data.cmd.cmd_flag_val;	\
			if ( ! (__mask & CMD_FLAG_REQUEST) )			\
				continue;					\
			if ( ( __val & CMD_FLAG_REQUEST ) != R_flag_val )	\
				continue;					\
			if (result)						\
				*result = _O(__li->next->o);			\
			goto end;						\
		}								\
		if (__cmp < 0)							\
			break;							\
	}									\
	if (result)								\
		*result = NULL;							\
	else									\
		ret = ENOENT;							\
}

static int search_vendor ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	vendor_id_t id;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case VENDOR_BY_ID:
			id = *(vendor_id_t *) what;
			SEARCH_scalar( id, &dict->dict_vendors.list[0], vendor.vendor_id, 1, &dict->dict_vendors );
			break;
				
		case VENDOR_BY_NAME:
			/* "what" is a vendor name */
			SEARCH_string( what, &dict->dict_vendors.list[0], vendor.vendor_name, 0);
			break;
			
		case VENDOR_OF_APPLICATION:
			/* "what" should be an application object */
			SEARCH_childs_parent( DICT_APPLICATION, &dict->dict_vendors );
			break;
		
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_application ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	application_id_t id;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case APPLICATION_BY_ID:
			id = *(application_id_t *) what;

			SEARCH_scalar( id, &dict->dict_applications.list[0],  application.application_id, 1, &dict->dict_applications );
			break;
				
		case APPLICATION_BY_NAME:
			/* "what" is an application name */
			SEARCH_string( what, &dict->dict_applications.list[0], application.application_name, 0);
			break;
			
		case APPLICATION_OF_TYPE:
			/* "what" should be a type object */
			SEARCH_childs_parent( DICT_TYPE, &dict->dict_applications );
			break;
		
		case APPLICATION_OF_COMMAND:
			/* "what" should be a command object */
			SEARCH_childs_parent( DICT_COMMAND, &dict->dict_applications );
			break;
		
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_type ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case TYPE_BY_NAME:
			/* "what" is a type name */
			SEARCH_string( what, &dict->dict_types, type.type_name, 1);
			break;
			
		case TYPE_OF_ENUMVAL:
			/* "what" should be a type_enum object */
			SEARCH_childs_parent( DICT_ENUMVAL, NULL );
			break;
		
		case TYPE_OF_AVP:
			/* "what" should be an avp object */
			SEARCH_childs_parent( DICT_AVP, NULL );
			break;
		
				
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_enumval ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case ENUMVAL_BY_STRUCT:
			{
				struct dict_object * parent = NULL;
				struct dict_enumval_request * _what = (struct dict_enumval_request *) what;
				
				CHECK_PARAMS(  _what  &&  ( _what->type_obj || _what->type_name )  );
				
				if (_what->type_obj != NULL) {
					parent = _what->type_obj;
					CHECK_PARAMS(  verify_object(parent)  &&  (parent->type == DICT_TYPE)  );
				} else {
					/* We received only the type name, we must find it first */
					CHECK_FCT_DO( search_type( dict, TYPE_BY_NAME, _what->type_name, &parent ),
							CHECK_PARAMS( 0 ) );
				}
				
				/* From here the "parent" object is valid */
				
				if ( _what->search.enum_name != NULL ) {
					/* We are looking for this string */
					SEARCH_string(  _what->search.enum_name, &parent->list[1], enumval.enum_name, 1 );
				} else {
					/* We are looking for the value in enum_value */
					switch (parent->data.type.type_base) {
						case AVP_TYPE_OCTETSTRING:
							SEARCH_ocstring( _what->search.enum_value.os.data, 
									 _what->search.enum_value.os.len, 
									 &parent->list[2], 
									 enumval.enum_value.os , 
									 1 );
							break;

						case AVP_TYPE_INTEGER32:
							SEARCH_scalar(	_what->search.enum_value.i32,
									&parent->list[2],
									enumval.enum_value.i32,
									1,
									(struct dict_object *)NULL);
							break;
							
						case AVP_TYPE_INTEGER64:
							SEARCH_scalar(	_what->search.enum_value.i64,
									&parent->list[2],
									enumval.enum_value.i64,
									1,
									(struct dict_object *)NULL);
							break;
							
						case AVP_TYPE_UNSIGNED32:
							SEARCH_scalar(	_what->search.enum_value.u32,
									&parent->list[2],
									enumval.enum_value.u32,
									1,
									(struct dict_object *)NULL);
							break;
							
						case AVP_TYPE_UNSIGNED64:
							SEARCH_scalar(	_what->search.enum_value.u64,
									&parent->list[2],
									enumval.enum_value.u64,
									1,
									(struct dict_object *)NULL);
							break;
							
						case AVP_TYPE_FLOAT32:
							SEARCH_scalar(	_what->search.enum_value.f32,
									&parent->list[2],
									enumval.enum_value.f32,
									1,
									(struct dict_object *)NULL);
							break;
							
						case AVP_TYPE_FLOAT64:
							SEARCH_scalar(	_what->search.enum_value.f64,
									&parent->list[2],
									enumval.enum_value.f64,
									1,
									(struct dict_object *)NULL);
							break;
							
						default:
							/* Invalid parent type basetype */
							CHECK_PARAMS( parent = NULL );
					}
				}
				
			}
			break;
		
				
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_avp ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case AVP_BY_CODE:
			{
				avp_code_t code;
				code = *(avp_code_t *) what;

				SEARCH_scalar( code, &dict->dict_vendors.list[1],  avp.avp_code, 1, (struct dict_object *)NULL );
			}
			break;
				
		case AVP_BY_NAME:
			/* "what" is the AVP name, vendor 0 */
			SEARCH_string( what, &dict->dict_vendors.list[2], avp.avp_name, 1);
			break;
			
		case AVP_BY_CODE_AND_VENDOR:
		case AVP_BY_NAME_AND_VENDOR:
			{
				struct dict_avp_request * _what = (struct dict_avp_request *) what;
				struct dict_object * vendor = NULL;
				
				CHECK_PARAMS( (criteria != AVP_BY_NAME_AND_VENDOR) || _what->avp_name  );
				
				/* Now look for the vendor first */
				CHECK_FCT( search_vendor( dict, VENDOR_BY_ID, &_what->avp_vendor, &vendor ) );
				if (vendor == NULL) {
					if (result)
						*result = NULL;
					else
						ret = ENOENT;
					goto end;
				}
				
				/* We now have our vendor = head of the appropriate avp list */
				if (criteria == AVP_BY_NAME_AND_VENDOR) {
					SEARCH_string( _what->avp_name, &vendor->list[2], avp.avp_name, 1);
				} else {
					/* AVP_BY_CODE_AND_VENDOR */
					SEARCH_scalar( _what->avp_code, &vendor->list[1],  avp.avp_code, 1, (struct dict_object *)NULL );
				}
			}
			break;
		
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_cmd ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case CMD_BY_NAME:
			/* "what" is a command name */
			SEARCH_string( what, &dict->dict_cmd_name, cmd.cmd_name, 1);
			break;
			
		case CMD_BY_CODE_R:
		case CMD_BY_CODE_A:
			{
				command_code_t code;
				uint8_t searchfl = 0;
				
				/* The command code that we are searching */
				code = *(command_code_t *) what;
				
				/* The flag (request or answer) of the command we are searching */
				if (criteria == CMD_BY_CODE_R) {
					searchfl = CMD_FLAG_REQUEST;
				}
				
				/* perform the search */
				SEARCH_codefl( code, searchfl, &dict->dict_cmd_code );
			}
			break;
				
		case CMD_ANSWER:
			{
				/* "what" is a command object of type "request" */
				struct dict_object * req = (struct dict_object *) what;
				struct dict_object * ans = NULL;
				
				CHECK_PARAMS( verify_object(req) 
						&& (req->type == DICT_COMMAND)
						&& (req->data.cmd.cmd_flag_mask & CMD_FLAG_REQUEST)
						&& (req->data.cmd.cmd_flag_val  & CMD_FLAG_REQUEST) );
				
				/* The answer is supposed to be the next element in the list, if it exists */
				ans = req->list[1].next->o;
				if ( ans == NULL ) {
					TRACE_DEBUG( FULL, "the request was the last element in the list" );
					ret = ENOENT;
					goto end;
				}
				
				/* Now check that the ans element is really the correct one */
				if (  (ans->data.cmd.cmd_code != req->data.cmd.cmd_code)
				   || (!(ans->data.cmd.cmd_flag_mask & CMD_FLAG_REQUEST))
				   || (  ans->data.cmd.cmd_flag_val  & CMD_FLAG_REQUEST ) ) {
					TRACE_DEBUG( FULL, "the answer does not follow the request in the list" );
					ret = ENOENT;
					goto end;
				}
				
				if (result)
					*result = ans;
				ret = 0;
			}						
			break;
			
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

static int search_rule ( struct dictionary * dict, int criteria, void * what, struct dict_object **result )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d %p %p", dict, criteria, what, result);
	
	switch (criteria) {
		case RULE_BY_AVP_AND_PARENT:
			{
				struct dict_object * parent = NULL;
				struct dict_object * avp = NULL;
				struct dict_rule_request * _what = (struct dict_rule_request *) what;
				
				CHECK_PARAMS( _what 
						&& (parent = _what->rule_parent)
						&& (avp    = _what->rule_avp   ) );
				
				CHECK_PARAMS( verify_object(parent) 
						&& ((parent->type == DICT_COMMAND) 
						 || ((parent->type == DICT_AVP) && (parent->data.avp.avp_basetype == AVP_TYPE_GROUPED))) );
				
				CHECK_PARAMS( verify_object(avp) && (avp->type == DICT_AVP) );
				
				/* Perform the search */
				SEARCH_ruleavpname( avp->data.avp.avp_name, &parent->list[2]);
				
			}
			break;
			
		default:
			/* Invalid criteria */
			CHECK_PARAMS( criteria = 0 );
	}
end:
	return ret;
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Dump / debug functions                                             */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/
/* The following functions are used to debug the module, and allow to print out the content of the dictionary */
static void dump_vendor_data ( void * data )
{
	struct dict_vendor_data * vendor = (struct dict_vendor_data *)data;
	
	fd_log_debug("data: %-6u \"%s\"", vendor->vendor_id, vendor->vendor_name);
}
static void dump_application_data ( void * data )
{
	struct dict_application_data * appli = (struct dict_application_data *) data;
	fd_log_debug("data: %-6u \"%s\"", appli->application_id, appli->application_name);
}
static void dump_type_data ( void * data )
{
	struct dict_type_data * type = ( struct dict_type_data * ) data;
	
	fd_log_debug("data: %-12s \"%s\"", 
			type_base_name[type->type_base], 
			type->type_name);
}
static void dump_enumval_data ( struct dict_enumval_data * enumval, enum dict_avp_basetype type )
{
	const int LEN_MAX = 20;
	fd_log_debug("data: (%-12s) \"%s\" -> ", type_base_name[type], enumval->enum_name);
	switch (type) {
		case AVP_TYPE_OCTETSTRING:
			{
				int i, n=LEN_MAX;
				if (enumval->enum_value.os.len < LEN_MAX)
					n = enumval->enum_value.os.len;
				for (i=0; i < n; i++)
					fd_log_debug("0x%02.2X/'%c' ", enumval->enum_value.os.data[i], ASCII(enumval->enum_value.os.data[i]));
				if (n == LEN_MAX)
					fd_log_debug("...");
			}
			break;
		
		case AVP_TYPE_INTEGER32:
			fd_log_debug("%i", enumval->enum_value.i32);
			break;

		case AVP_TYPE_INTEGER64:
			fd_log_debug("%lli", enumval->enum_value.i64);
			break;

		case AVP_TYPE_UNSIGNED32:
			fd_log_debug("%u", enumval->enum_value.u32);
			break;

		case AVP_TYPE_UNSIGNED64:
			fd_log_debug("%llu", enumval->enum_value.u64);
			break;

		case AVP_TYPE_FLOAT32:
			fd_log_debug("%f", enumval->enum_value.f32);
			break;

		case AVP_TYPE_FLOAT64:
			fd_log_debug("%g", enumval->enum_value.f64);
			break;
		
		default:
			fd_log_debug("??? (ERROR unknown type %d)", type);
	}
}
static void dump_avp_data ( void * data )
{
	struct dict_avp_data * avp = (struct dict_avp_data * ) data;
	fd_log_debug("data: v/m:" DUMP_AVPFL_str "/" DUMP_AVPFL_str ", %12s, %-6u \"%s\"", 
			DUMP_AVPFL_val(avp->avp_flag_val), 
			DUMP_AVPFL_val(avp->avp_flag_mask), 
			type_base_name[avp->avp_basetype], 
			avp->avp_code, 
			avp->avp_name );
}
static void dump_command_data ( void * data )
{
	struct dict_cmd_data * cmd = (struct dict_cmd_data *) data;
	fd_log_debug("data: v/m:" DUMP_CMDFL_str "/" DUMP_CMDFL_str ", %-6u \"%s\"", 
			DUMP_CMDFL_val(cmd->cmd_flag_val), DUMP_CMDFL_val(cmd->cmd_flag_mask), cmd->cmd_code, cmd->cmd_name);
}
static void dump_rule_data ( void * data )
{
	struct dict_rule_data * rule = (struct dict_rule_data * )data;
	fd_log_debug("data: pos:%d ord:%d m/M:%2d/%2d avp:\"%s\"", 
			rule->rule_position, 
			rule->rule_order, 
			rule->rule_min, 
			rule->rule_max,
			rule->rule_avp->data.avp.avp_name);
}

static void dump_object ( struct dict_object * obj, int parents, int depth, int indent );

static void dump_list ( struct fd_list * sentinel, int parents, int depth, int indent )
{
	struct fd_list * li = sentinel;
	/* We don't lock here, the caller must have taken the dictionary lock for reading already */
	while (li->next != sentinel)
	{
		li = li->next;
		dump_object( _O(li->o), parents, depth, indent );
	}
}

static void dump_object ( struct dict_object * obj, int parents, int depth, int indent )
{
	if (obj == NULL)
		return;
	
	if (parents)
		dump_object (obj->parent, parents-1, 0, indent + 1 );
	
	fd_log_debug("%*s@%p: %s%s (p:%-9p) ", 
			indent,
			"",
			obj, 
			verify_object(obj) ? "" : "INVALID ", 
			_OBINFO(obj).name, 
			obj->parent);
	
	if (obj->type == DICT_ENUMVAL)
		dump_enumval_data ( &obj->data.enumval, obj->parent->data.type.type_base );
	else
		_OBINFO(obj).dump_data(&obj->data);
	
	fd_log_debug("\n");
	
	if (depth) {
		int i;
		for (i=0; i<NB_LISTS_PER_OBJ; i++) {
			if ((obj->list[i].o == NULL) && (obj->list[i].next != &obj->list[i])) {
				fd_log_debug("%*s>%p: list[%d]:\n", indent, "", obj, i);
				dump_list(&obj->list[i], parents, depth - 1, indent + 2);
			}
		}
	}
}

void fd_dict_dump_object(struct dict_object * obj)
{
	fd_log_debug("Dictionary object %p dump:\n", obj);
	dump_object( obj, 1, 2, 2 );
}

void fd_dict_dump(struct dictionary * dict)
{
	int i;
	
	CHECK_PARAMS_DO(dict && (dict->dict_eyec == DICT_EYECATCHER), return);
	
	CHECK_POSIX_DO(  pthread_rwlock_rdlock( &dict->dict_lock ), /* ignore */  );
	
	fd_log_debug("######################################################\n");
	fd_log_debug("###### Dumping vendors, AVPs and related rules #######\n");
	
	dump_object( &dict->dict_vendors, 0, 3, 0 );
	
	fd_log_debug("###### Dumping applications #######\n");

	dump_object( &dict->dict_applications, 0, 1, 0 );
	
	fd_log_debug("###### Dumping types #######\n");

	dump_list( &dict->dict_types, 0, 2, 0 );
	
	fd_log_debug("###### Dumping commands per name #######\n");

	dump_list( &dict->dict_cmd_name, 0, 2, 0 );
	
	fd_log_debug("###### Dumping commands per code and flags #######\n");

	dump_list( &dict->dict_cmd_code, 0, 0, 0 );
	
	fd_log_debug("###### Statistics #######\n");

	for (i=1; i<=DICT_TYPE_MAX; i++)
		fd_log_debug(" %5d objects of type %s\n", dict->dict_count[i], dict_obj_info[i].name);
	
	fd_log_debug("######################################################\n");
	
	/* Free the rwlock */
	CHECK_POSIX_DO(  pthread_rwlock_unlock( &dict->dict_lock ), /* ignore */  );
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Exported functions                                                 */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* These are the functions exported outside libfreeDiameter. */

/* Get the data associated to an object */
int fd_dict_gettype ( struct dict_object * object, enum dict_object_type * type)
{
	TRACE_ENTRY("%p %p", object, type);
	
	CHECK_PARAMS( type && verify_object(object) );
	
	/* Copy the value and return */
	*type = object->type;
	return 0;
}

int fd_dict_getdict ( struct dict_object * object, struct dictionary ** dict)
{
	TRACE_ENTRY("%p %p", object, dict);
	
	CHECK_PARAMS( dict && verify_object(object) );
	
	/* Copy the value and return */
	*dict = object->dico;
	return 0;
}


/* Get the data associated to an object */
int fd_dict_getval ( struct dict_object * object, void * val)
{
	TRACE_ENTRY("%p %p", object, val);
	
	CHECK_PARAMS( val && verify_object(object) );
	
	/* Copy the value and return */
	memcpy(val, &object->data, _OBINFO(object).datasize);;
	return 0;
}

/* Add a new object in the dictionary */
int fd_dict_new ( struct dictionary * dict, enum dict_object_type type, void * data, struct dict_object * parent, struct dict_object **ref )
{
	int ret = 0;
	struct dict_object * new = NULL;
	struct dict_object * vendor = NULL;
	
	TRACE_ENTRY("%p %d(%s) %p %p %p", dict, type, dict_obj_info[CHECK_TYPE(type) ? type : 0].name, data, parent, ref);
	
	/* Check parameters */
	CHECK_PARAMS( dict && (dict->dict_eyec == DICT_EYECATCHER) && CHECK_TYPE(type) && data  );
	
	/* Check the "parent" parameter */
	switch (dict_obj_info[type].parent) {
		case 0:	/* parent is forbidden */
			CHECK_PARAMS( parent == NULL );
		
		case 1:	/* parent is optional */
			if (parent == NULL)
				break;
		
		case 2: /* parent is mandatory */
			CHECK_PARAMS(  verify_object(parent)  );
			
			if (type == DICT_RULE ) { /* Special case : grouped AVP or Command parents are allowed */
				CHECK_PARAMS( (parent->type == DICT_COMMAND ) 
						|| ( (parent->type == DICT_AVP) && (parent->data.avp.avp_basetype == AVP_TYPE_GROUPED ) ) );
			} else {
				CHECK_PARAMS( parent->type == dict_obj_info[type].parenttype );
			}
	}
	
	/* For AVP object, we must also check that the "vendor" referenced exists */
	if (type == DICT_AVP) {
		CHECK_FCT_DO(  fd_dict_search( dict, DICT_VENDOR, VENDOR_BY_ID, &(((struct dict_avp_data *)data)->avp_vendor), (void*)&vendor, ENOENT ),
				CHECK_PARAMS( vendor = NULL )  );
		
		/* Also check if a parent is provided, that the type are the same */
		if (parent) {
			CHECK_PARAMS(  parent->data.type.type_base == ((struct dict_avp_data *)data)->avp_basetype  );
		}
	}
	
	/* For RULE object, we must also check that the "avp" referenced exists */
	if (type == DICT_RULE) {
		CHECK_PARAMS(  verify_object(((struct dict_rule_data *)data)->rule_avp)  );
		CHECK_PARAMS(  ((struct dict_rule_data *)data)->rule_avp->type == DICT_AVP  );
	}
	
	/* For COMMAND object, check that the 'R' flag is fixed */
	if (type == DICT_COMMAND) {
		CHECK_PARAMS( ((struct dict_cmd_data *)data)->cmd_flag_mask & CMD_FLAG_REQUEST   );
	}
	
	/* Parameters are valid, create the new object */
	CHECK_MALLOC(  new = malloc(sizeof(struct dict_object))  );
	
	/* Initialize the data of the new object */
	init_object(new, type);
	init_object_data(&new->data, data, type);
	new->dico = dict;
	new->parent = parent;
	
	/* We will change the dictionary => acquire the write lock */
	CHECK_POSIX_DO(  ret = pthread_rwlock_wrlock(&dict->dict_lock),  goto error_free  );
	
	/* Now link the object -- this also checks that no object with same keys already exists */
	switch (type) {
		case DICT_VENDOR:
			/* A vendor object is linked in the g_dict_vendors.list[0], by their id */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &dict->dict_vendors.list[0], &new->list[0], (int (*)(void*, void *))order_vendor_by_id, (void **)ref ),
					goto error_unlock  );
			break;
		
		case DICT_APPLICATION:
			/* An application object is linked in the g_dict_applciations.list[0], by their id */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &dict->dict_applications.list[0], &new->list[0], (int (*)(void*, void *))order_appli_by_id, (void **)ref ),
					goto error_unlock  );
			break;
		
		case DICT_TYPE:
			/* A type object is linked in g_list_types by its name */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &dict->dict_types, &new->list[0], (int (*)(void*, void *))order_type_by_name, (void **)ref ),
					goto error_unlock  );
			break;
		
		case DICT_ENUMVAL:
			/* A type_enum object is linked in it's parent 'type' object lists 1 and 2 by its name and values */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &parent->list[1], &new->list[0], (int (*)(void*, void *))order_enum_by_name, (void **)ref ),
					goto error_unlock  );
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &parent->list[2], &new->list[1], (int (*)(void*, void *))order_enum_by_val, (void **)ref ),
					{ fd_list_unlink(&new->list[0]); goto error_unlock; }  );
			break;
		
		case DICT_AVP:
			/* An avp object is linked in lists 1 and 2 of its vendor, by code and name */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &vendor->list[1], &new->list[0], (int (*)(void*, void *))order_avp_by_code, (void **)ref ),
					goto error_unlock  );
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &vendor->list[2], &new->list[1], (int (*)(void*, void *))order_avp_by_name, (void **)ref ),
					{ fd_list_unlink(&new->list[0]); goto error_unlock; }  );
			break;
			
		case DICT_COMMAND:
			/* A command object is linked in g_list_cmd_name and g_list_cmd_code by its name and code */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &dict->dict_cmd_code, &new->list[1], (int (*)(void*, void *))order_cmd_by_codefl, (void **)ref ),
					goto error_unlock  );
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &dict->dict_cmd_name, &new->list[0], (int (*)(void*, void *))order_cmd_by_name, (void **)ref ),
					{ fd_list_unlink(&new->list[1]); goto error_unlock; }  );
			break;
		
		case DICT_RULE:
			/* A rule object is linked in list[2] of its parent command or AVP by the name of the AVP it refers */
			CHECK_FCT_DO(  ret = fd_list_insert_ordered ( &parent->list[2], &new->list[0], (int (*)(void*, void *))order_rule_by_avpn, (void **)ref ),
					goto error_unlock  );
			break;
			
		default:
			ASSERT(0);
	}
	
	/* A new object has been created, increment the global counter */
	dict->dict_count[type]++;
	
	/* Unlock the dictionary */
	CHECK_POSIX_DO(  ret = pthread_rwlock_unlock(&dict->dict_lock),  goto error_free  );
	
	/* Save the pointer to the new object */
	if (ref)
		*ref = new;
	
	return 0;
	
error_unlock:
	CHECK_POSIX_DO(  pthread_rwlock_unlock(&dict->dict_lock),  /* continue */  );
error_free:
	free(new);
	return ret;
}

int fd_dict_search ( struct dictionary * dict, enum dict_object_type type, int criteria, void * what, struct dict_object **result, int retval )
{
	int ret = 0;
	
	TRACE_ENTRY("%p %d(%s) %d %p %p %d", dict, type, dict_obj_info[CHECK_TYPE(type) ? type : 0].name, criteria, what, result, retval);
	
	/* Check param */
	CHECK_PARAMS( dict && (dict->dict_eyec == DICT_EYECATCHER) && CHECK_TYPE(type) );
	
	/* Lock the dictionary for reading */
	CHECK_POSIX(  pthread_rwlock_rdlock(&dict->dict_lock)  );
	
	/* Now call the type-specific search function */
	ret = dict_obj_info[type].search_fct (dict, criteria, what, result);
	
	/* Unlock */
	CHECK_POSIX(  pthread_rwlock_unlock(&dict->dict_lock)  );
	
	/* Update the return value as needed */
	if ((result != NULL) && (*result == NULL))
		ret = retval;
	
	return ret;
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  The init/fini functions                                            */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* Initialize the dictionary */
int fd_dict_init ( struct dictionary ** dict)
{
	struct dictionary * new = NULL;
	
	TRACE_ENTRY("");
	
	/* Sanity checks */
	ASSERT( (sizeof(type_base_name) / sizeof(type_base_name[0])) == (AVP_TYPE_MAX + 1) );
	ASSERT( (sizeof(dict_obj_info)  / sizeof(dict_obj_info[0]))  == (DICT_TYPE_MAX + 1) );
	CHECK_PARAMS(dict);
	
	/* Allocate the memory for the dictionary */
	CHECK_MALLOC( new = malloc(sizeof(struct dictionary)) );
	memset(new, 0, sizeof(struct dictionary));
	
	new->dict_eyec = DICT_EYECATCHER;
	
	/* Initialize the lock for the dictionary */
	CHECK_POSIX(  pthread_rwlock_init(&new->dict_lock, NULL)  );
	
	/* Initialize the sentinel for vendors and AVP lists */
	init_object( &new->dict_vendors, DICT_VENDOR );
	new->dict_vendors.data.vendor.vendor_name = "(no vendor)";
	new->dict_vendors.list[0].o = NULL; /* overwrite since element is also sentinel for this list. */
	
	
	/* Initialize the sentinel for applciations */
	init_object( &new->dict_applications, DICT_APPLICATION );
	new->dict_applications.data.application.application_name = "Diameter Common Messages";
	new->dict_applications.list[0].o = NULL; /* overwrite since since element is also sentinel for this list. */
			
	/* Initialize the sentinel for types */
	fd_list_init ( &new->dict_types, NULL );
	
	/* Initialize the sentinels for commands */
	fd_list_init ( &new->dict_cmd_name, NULL );
	fd_list_init ( &new->dict_cmd_code, NULL );
	
	/* Initialize the error command object */
	init_object( &new->dict_cmd_error, DICT_COMMAND );
	new->dict_cmd_error.data.cmd.cmd_name="(generic error format)";
	new->dict_cmd_error.data.cmd.cmd_flag_mask=CMD_FLAG_ERROR | CMD_FLAG_REQUEST | CMD_FLAG_RETRANSMIT;
	new->dict_cmd_error.data.cmd.cmd_flag_val =CMD_FLAG_ERROR;
	
	*dict = new;
	
	/* Done */
	return 0;
}

/* Destroy a dictionary */
int fd_dict_fini ( struct dictionary ** dict)
{
	int i;
	
	TRACE_ENTRY("");
	CHECK_PARAMS( dict && *dict && ((*dict)->dict_eyec == DICT_EYECATCHER) );
	
	/* Acquire the write lock to make sure no other operation is ongoing */
	CHECK_POSIX(  pthread_rwlock_wrlock(&(*dict)->dict_lock)  );
	
	/* Empty all the lists, free the elements */
	destroy_list ( &(*dict)->dict_cmd_error.list[2] );
	destroy_list ( &(*dict)->dict_cmd_code );
	destroy_list ( &(*dict)->dict_cmd_name );
	destroy_list ( &(*dict)->dict_types );
	for (i=0; i< NB_LISTS_PER_OBJ; i++) {
		destroy_list ( &(*dict)->dict_applications.list[i] );
		destroy_list ( &(*dict)->dict_vendors.list[i] );
	}
	
	/* Dictionary is empty, now destroy the lock */
	CHECK_POSIX(  pthread_rwlock_unlock(&(*dict)->dict_lock)  );
	CHECK_POSIX(  pthread_rwlock_destroy(&(*dict)->dict_lock)  );
	
	free(*dict);
	*dict = NULL;
	
	return 0;
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*                                                                                                     */
/*                                  Other functions                                                    */
/*                                                                                                     */
/*******************************************************************************************************/
/*******************************************************************************************************/

/* Iterate a callback on the rules for an object */
int fd_dict_iterate_rules ( struct dict_object *parent, void * data, int (*cb)(void *, struct dict_rule_data *) )
{
	int ret = 0;
	struct fd_list * li;
	
	TRACE_ENTRY("%p %p %p", parent, data, cb);
	
	/* Check parameters */
	CHECK_PARAMS(  verify_object(parent)  );
	CHECK_PARAMS(  (parent->type == DICT_COMMAND) 
			|| ((parent->type == DICT_AVP) && (parent->data.avp.avp_basetype == AVP_TYPE_GROUPED)) );
	TRACE_DEBUG (FULL, "Iterating on rules of %s: '%s'.", 
			_OBINFO(parent).name, 
			parent->type == DICT_COMMAND ? 
				  parent->data.cmd.cmd_name
				: parent->data.avp.avp_name);
	
	/* Acquire the read lock  */
	CHECK_POSIX(  pthread_rwlock_rdlock(&parent->dico->dict_lock)  );
	
	/* go through the list and call the cb on each rule data */
	for (li = &(parent->list[2]); li->next != &(parent->list[2]); li = li->next) {
		ret = (*cb)(data, &(_O(li->next->o)->data.rule));
		if (ret != 0)
			break;
	}
		
	/* Release the lock */
	CHECK_POSIX(  pthread_rwlock_unlock(&parent->dico->dict_lock)  );
	
	return ret;
}

/* Create the list of vendors. Returns a 0-terminated array, that must be freed after use. Returns NULL on error. */
uint32_t * fd_dict_get_vendorid_list(struct dictionary * dict)
{
	uint32_t * ret = NULL;
	int i = 0;
	struct fd_list * li;
	
	TRACE_ENTRY();
	
	/* Acquire the read lock */
	CHECK_POSIX_DO(  pthread_rwlock_rdlock(&dict->dict_lock), return NULL  );
	
	/* Allocate an array to contain all the elements */
	CHECK_MALLOC_DO( ret = calloc( dict->dict_count[DICT_VENDOR] + 1, sizeof(uint32_t) ), goto out );
	
	/* Copy the vendors IDs */
	for (li = dict->dict_vendors.list[0].next; li != &(dict->dict_vendors.list[0]); li = li->next) {
		ret[i] = _O(li->o)->data.vendor.vendor_id;
		i++;
		ASSERT( i <= dict->dict_count[DICT_VENDOR] );
	}
out:	
	/* Release the lock */
	CHECK_POSIX_DO(  pthread_rwlock_unlock(&dict->dict_lock), return NULL  );
	
	return ret;
}

/* Return the location of the cb list for an object, after checking its type */
int fd_dict_disp_cb(enum dict_object_type type, struct dict_object *obj, struct fd_list ** cb_list)
{
	TRACE_ENTRY("%d %p %p", type, obj, cb_list);
	CHECK_PARAMS( verify_object(obj) );
	CHECK_PARAMS( _OBINFO(obj).type == type );
	CHECK_PARAMS( cb_list );
	*cb_list = &obj->disp_cbs;
	return 0;
}

int fd_dict_get_error_cmd(struct dictionary * dict, struct dict_object **obj)
{
	TRACE_ENTRY("%p %p", dict, obj);
	CHECK_PARAMS( dict && (dict->dict_eyec == DICT_EYECATCHER) && obj );
	*obj = &dict->dict_cmd_error;
	return 0;
}
