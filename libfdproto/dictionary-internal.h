/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2020, WIDE Project and NICT								 *
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

#ifndef HAD_DICTIONARY_INTERNAL_H
#define HAD_DICTIONARY_INTERNAL_H

/* Names of the base types */
extern const char * type_base_name[];

/* The number of lists in an object */
#define NB_LISTS_PER_OBJ	3

/* Definition of the dictionary objects */
struct dict_object {
	enum dict_object_type	type;	/* What type of object is this? */
	int			objeyec;/* eyecatcher for this object */
	int			typeyec;/* eyecatcher for this type of object */
	struct dictionary	*dico;  /* The dictionary this object belongs to */

	union {
		struct dict_vendor_data		vendor;		/* datastr_len = strlen(vendor_name) */
		struct dict_application_data	application;	/* datastr_len = strlen(application_name) */
		struct dict_type_data		type;		/* datastr_len = strlen(type_name) */
		struct dict_enumval_data	enumval;	/* datastr_len = strlen(enum_name) */
		struct dict_avp_data		avp;		/* datastr_len = strlen(avp_name) */
		struct dict_cmd_data		cmd;		/* datastr_len = strlen(cmd_name) */
		struct dict_rule_data		rule;		/* datastr_len = 0 */
	} data;				/* The data of this object */

	size_t			datastr_len; /* cached length of the string inside the data. Saved when the object is created. */

	struct dict_object *	parent; /* The parent of this object, if any */

	struct fd_list		list[NB_LISTS_PER_OBJ];/* used to chain objects.*/
	/* More information about the lists :

	 - the use for each list depends on the type of object. See detail below.

	 - a sentinel for a list has its 'o' field cleared. (this is the criteria to detect end of a loop)

	 - The lists are always ordered. The criteria are described below. the functions to order them are referenced in dict_obj_info

	 - The dict_lock must be held for any list operation.

	 => VENDORS:
	 list[0]: list of the vendors, ordered by their id. The sentinel is g_dict_vendors (vendor with id 0)
	 list[1]: sentinel for the list of AVPs from this vendor, ordered by AVP code.
	 list[2]: sentinel for the list of AVPs from this vendor, ordered by AVP name (fd_os_cmp).

	 => APPLICATIONS:
	 list[0]: list of the applications, ordered by their id. The sentinel is g_dict_applications (application with id 0)
	 list[1]: not used
	 list[2]: not used.

	 => TYPES:
	 list[0]: list of the types, ordered by their names. The sentinel is g_list_types.
	 list[1]: sentinel for the type_enum list of this type, ordered by their constant name (fd_os_cmp).
	 list[2]: sentinel for the type_enum list of this type, ordered by their constant value.

	 => TYPE_ENUMS:
	 list[0]: list of the contants for a given type, ordered by the constant name (fd_os_cmp). Sentinel is a (list[1]) element of a TYPE object.
	 list[1]: list of the contants for a given type, ordered by the constant value. Sentinel is a (list[2]) element of a TYPE object.
	 list[2]: not used

	 => AVPS:
	 list[0]: list of the AVP from a given vendor, ordered by avp code. Sentinel is a list[1] element of a VENDOR object.
	 list[1]: list of the AVP from a given vendor, ordered by avp name (fd_os_cmp). Sentinel is a list[2] element of a VENDOR object.
	 list[2]: sentinel for the rule list that apply to this AVP.

	 => COMMANDS:
	 list[0]: list of the commands, ordered by their names (fd_os_cmp). The sentinel is g_list_cmd_name.
	 list[1]: list of the commands, ordered by their command code and 'R' flag. The sentinel is g_list_cmd_code.
	 list[2]: sentinel for the rule list that apply to this command.

	 => RULES:
	 list[0]: list of the rules for a given (grouped) AVP or Command, ordered by the AVP vendor & code to which they refer. sentinel is list[2] of a command or (grouped) avp.
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

	int			dict_count[DICT_TYPE_MAX + 1]; /* Number of objects of each type */
};

#endif /* HAD_DICTIONARY_INTERNAL_H */
