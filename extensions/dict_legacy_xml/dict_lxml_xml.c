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

#include "dict_lxml.h"

/*
The internal freeDiameter dictionary has strong dependency relations between
the different objects, as follow:

           vendor
           /    \
   application   \
     /    \      |
 command   \     |
  |       type   |
  |       /   \  |
   \  enumval  \ | 
    \          avp
     \    _____/    
      \  /
      rule

It means an AVP cannot be defined unless the parent TYPE has already been defined, 
in turn depending on parent APPLICATION, etc. (top-to-bottom dependencies on the graph)

On the other hand, the hierarchy of the XML format described in draft-frascone-xml-dictionary-00 
does not enforce most of these dependencies, the structure is as follows:

 vendor     application
	   /     |     \
      command    |     avp 
	 /      type     \
       rule	        enumval

(in addition if DTD validation was performed, command and avp refer to vendor, avp refers to type, 
but we do not do it for larger compatibility -- we just report when errors are found)

As a consequence of this difference, it is impossible to parse the XML tree and create the dictionary objects in freeDiameter
in only 1 pass. To avoid parsing the tree several times, we use a temporary structure in memory to contain all the data
from the XML file, and when the parsing is complete we store all the objects in the dictionary. 
*/

/* We use the SAX interface of libxml2 (from GNOME) to parse the XML file. */ 
#include <libxml/parser.h>

/*******************************************/
 /* Helper functions */
static int xmltoint(xmlChar * xmlinteger, uint32_t * conv) {
	TRACE_ENTRY("%p %p", xmlinteger, conv);
	
	/* Attempt at converting the string to an integer */
	if (sscanf((char *)xmlinteger, "%u", conv) != 1) {
		TRACE_DEBUG(INFO, "Unable to convert '%s' to integer.", (char *)xmlinteger)
		return EINVAL;
	}
	
	return 0;
}


/*******************************************
 The temporary structure that is being built when the XML file is parsed 
 *******************************************/

/* VENDOR */
struct t_vend {
	struct fd_list 	chain; /* link in the t_dictionary->vendors */
	uint32_t	id;
	uint8_t *	name;
};

static int new_vendor(struct fd_list * parent, xmlChar * xmlid, xmlChar * xmlname) {
	struct t_vend * new;
	uint32_t id = 0;
	
	TRACE_ENTRY("%p %p %p", parent, xmlid, xmlname);
	CHECK_PARAMS( parent && xmlid && xmlname );
	
	CHECK_FCT( xmltoint(xmlid, &id) );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_vend)) );
	memset(new, 0, sizeof(struct t_vend));
	fd_list_init(&new->chain, NULL);
	new->id = id;
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	
	fd_list_insert_before(parent, &new->chain);
	
	return 0;
}

static void dump_vendor(struct t_vend * v) {
	fd_log_debug(" Vendor %d:'%s'\n", v->id, (char *)v->name);
}

static void del_vendor_contents(struct t_vend * v) {
	TRACE_ENTRY("%p", v);
	free(v->name);
}


/* RULE */
struct t_rule {
	struct fd_list  chain;	/* link in either t_cmd or t_avp */
	uint8_t *	avpname;
	int		max;
	int		min;
};

static int new_rule(struct fd_list * parent, xmlChar * xmlname, /* position is never used */ xmlChar * xmlmaximum, xmlChar * xmlminimum) {
	struct t_rule * new;
	uint32_t min, max;
	
	TRACE_ENTRY("%p %p %p %p", parent, xmlname, xmlmaximum, xmlminimum);
	CHECK_PARAMS( parent && xmlname );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_rule)) );
	memset(new, 0, sizeof(struct t_rule));
	fd_list_init(&new->chain, NULL);
	if (xmlminimum) {
		CHECK_FCT( xmltoint(xmlminimum, &min) );
		new->min = (int) min;
	} else {
		new->min = -1;
	}
	if (xmlmaximum) {
		CHECK_FCT( xmltoint(xmlmaximum, &max) );
		new->max = (int) max;
	} else {
		new->max = -1;
	}
	CHECK_MALLOC( new->avpname = (uint8_t *)strdup((char *)xmlname) );
	
	fd_list_insert_before(parent, &new->chain);
	
	return 0;
}

static void dump_rule(struct t_rule * r, char * prefix) {
	fd_log_debug("%s ", prefix);
	if (r->min != -1)
		fd_log_debug("m:%d ", r->min);
	if (r->max != -1)
		fd_log_debug("M:%d ", r->max);
	fd_log_debug("%s\n", (char *)r->avpname);
}

static void del_rule_contents(struct t_rule * r) {
	TRACE_ENTRY("%p",r);
	free(r->avpname);
}


/* COMMAND */
struct t_cmd {
	struct fd_list  chain;    /* link in t_appl->commands */
	uint32_t	code;
	uint8_t *	name;
	uint32_t	flags;
	uint32_t	fmask;
	struct fd_list  reqrules_fixed;     /* list of t_rule */
	struct fd_list  reqrules_required;  /* list of t_rule */
	struct fd_list  reqrules_optional;  /* list of t_rule */
	struct fd_list  ansrules_fixed;     /* list of t_rule */
	struct fd_list  ansrules_required;  /* list of t_rule */
	struct fd_list  ansrules_optional;  /* list of t_rule */
};

static int new_cmd(struct fd_list * parent, xmlChar * xmlcode, xmlChar * xmlname /*, ignore the vendor id because we don't use it */, xmlChar * xmlpbit, struct t_cmd **ret) {
	struct t_cmd * new;
	uint32_t code;
	uint32_t flag = 0;
	uint32_t fmask = 0;
	
	TRACE_ENTRY("%p %p %p %p", parent, xmlcode, xmlname, xmlpbit);
	CHECK_PARAMS( parent && xmlcode && xmlname );
	
	CHECK_FCT( xmltoint(xmlcode, &code) );
	
	if (xmlpbit) {
		uint32_t val;
		CHECK_FCT( xmltoint(xmlpbit, &val) );
		fmask |= CMD_FLAG_PROXIABLE;
		if (val)
			flag |= CMD_FLAG_PROXIABLE;
	}
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_cmd)) );
	memset(new, 0, sizeof(struct t_cmd));
	fd_list_init(&new->chain, NULL);
	new->code = code;
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	new->flags = flag;
	new->fmask = fmask;
	fd_list_init(&new->reqrules_fixed, NULL);
	fd_list_init(&new->reqrules_required, NULL);
	fd_list_init(&new->reqrules_optional, NULL);
	fd_list_init(&new->ansrules_fixed, NULL);
	fd_list_init(&new->ansrules_required, NULL);
	fd_list_init(&new->ansrules_optional, NULL);
	
	fd_list_insert_before(parent, &new->chain);
	
	*ret = new;
	
	return 0;
}

static void dump_cmd(struct t_cmd * c) {
	struct fd_list * li;
	fd_log_debug("  Command %d %s: %s\n", c->code, 
		c->fmask ? ( c->flags ? "[P=1]" : "[P=0]") : "", c->name);
	for (li = c->reqrules_fixed.next; li != &c->reqrules_fixed; li = li->next)
		dump_rule((struct t_rule *)li, "    Request fixed    AVP:");
	for (li = c->reqrules_required.next; li != &c->reqrules_required; li = li->next)
		dump_rule((struct t_rule *)li, "    Request required AVP:");
	for (li = c->reqrules_optional.next; li != &c->reqrules_optional; li = li->next)
		dump_rule((struct t_rule *)li, "    Request optional AVP:");
	for (li = c->ansrules_fixed.next; li != &c->ansrules_fixed; li = li->next)
		dump_rule((struct t_rule *)li, "    Answer fixed    AVP:");
	for (li = c->ansrules_required.next; li != &c->ansrules_required; li = li->next)
		dump_rule((struct t_rule *)li, "    Answer required AVP:");
	for (li = c->ansrules_optional.next; li != &c->ansrules_optional; li = li->next)
		dump_rule((struct t_rule *)li, "    Answer optional AVP:");
}

static void del_cmd_contents(struct t_cmd * c) {
	TRACE_ENTRY("%p", c);
	free(c->name);
	while (!FD_IS_LIST_EMPTY(&c->reqrules_fixed)) {
		struct fd_list * li = c->reqrules_fixed.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&c->reqrules_required)) {
		struct fd_list * li = c->reqrules_required.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&c->reqrules_optional)) {
		struct fd_list * li = c->reqrules_optional.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&c->ansrules_fixed)) {
		struct fd_list * li = c->ansrules_fixed.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&c->ansrules_required)) {
		struct fd_list * li = c->ansrules_required.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&c->ansrules_optional)) {
		struct fd_list * li = c->ansrules_optional.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
}

/* TYPE */
struct t_typedefn {
	struct fd_list  chain;	/* link in t_appl->types */
	uint8_t *	name;
	uint8_t *	parent_name;
};

static int new_type(struct fd_list * parent, xmlChar * xmlname, xmlChar * xmlparent /*, xmlChar * xmldescription -- ignore */) {
	struct t_typedefn * new;
	
	TRACE_ENTRY("%p %p %p", parent, xmlname, xmlparent);
	CHECK_PARAMS( parent && xmlname );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_typedefn)) );
	memset(new, 0, sizeof(struct t_typedefn));
	fd_list_init(&new->chain, NULL);
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	if (xmlparent) {
		CHECK_MALLOC( new->parent_name = (uint8_t *)strdup((char *)xmlparent) );
	}
	
	fd_list_insert_before(parent, &new->chain);
	
	return 0;
}

static void dump_type(struct t_typedefn * t) {
	fd_log_debug("  Type %s", (char *)t->name);
	if (t->parent_name)
		fd_log_debug("(parent: %s)", (char *)t->parent_name);
	fd_log_debug("\n");
}

static void del_type_contents(struct t_typedefn * t) {
	TRACE_ENTRY("%p", t);
	free(t->name);
	free(t->parent_name);
}
	

/* TYPE INSIDE AVP */
struct t_avptype {
	struct fd_list  chain;  /* link in t_avp->type */
	uint8_t *	type_name;
};

static int new_avptype(struct fd_list * parent, xmlChar * xmlname) {
	struct t_avptype * new;
	
	TRACE_ENTRY("%p %p", parent, xmlname);
	CHECK_PARAMS( parent && xmlname );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_avptype)) );
	memset(new, 0, sizeof(struct t_avptype));
	fd_list_init(&new->chain, NULL);
	CHECK_MALLOC( new->type_name = (uint8_t *)strdup((char *)xmlname) );
	
	fd_list_insert_before(parent, &new->chain);
	
	return 0;
}

static void dump_avptype(struct t_avptype * t) {
	fd_log_debug("    data type: %s\n", t->type_name);
}

static void del_avptype_contents(struct t_avptype * t) {
	TRACE_ENTRY("%p", t);
	free(t->type_name);
}
	

/* ENUM */
struct t_enum {
	struct fd_list  chain;  /* link in t_avp->enums */
	uint32_t	code;
	uint8_t *	name;
};

static int new_enum(struct fd_list * parent, xmlChar * xmlcode, xmlChar * xmlname) {
	struct t_enum * new;
	uint32_t code = 0;
	
	TRACE_ENTRY("%p %p %p", parent, xmlcode, xmlname);
	CHECK_PARAMS( parent && xmlcode && xmlname );
	
	CHECK_FCT( xmltoint(xmlcode, &code) );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_enum)) );
	memset(new, 0, sizeof(struct t_enum));
	fd_list_init(&new->chain, NULL);
	new->code = code;
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	
	fd_list_insert_before(parent, &new->chain);
	
	return 0;
}

static void dump_enum(struct t_enum * e) {
	fd_log_debug("    Value: %d == %s\n", e->code, e->name);
}	

static void del_enum_contents(struct t_enum * e) {
	TRACE_ENTRY("%p", e);
	free(e->name);
}

/* AVP */
struct t_avp {
	struct fd_list  chain;  /* link in t_appl->avps */
	uint32_t	code;
	uint8_t *	name;
	uint32_t	flags;
	uint32_t	fmask;
	uint32_t	vendor;
	struct fd_list  type;             /* list of t_avptype -- there must be at max 1 item in the list */
	struct fd_list  enums;            /* list of t_enum */
	struct fd_list  grouped_fixed;    /* list of t_rule */
	struct fd_list  grouped_required; /* list of t_rule */
	struct fd_list  grouped_optional; /* list of t_rule */
};

static int new_avp(struct fd_list * parent, xmlChar * xmlcode, xmlChar * xmlname, xmlChar * xmlmandatory, xmlChar * xmlvendor, struct t_avp **ret) {
	/* we ignore description, may-encrypt, protected, ... */
	struct t_avp * new;
	uint32_t code;
	uint32_t vendor = 0;
	uint32_t flag = 0;
	uint32_t fmask = 0;
	
	TRACE_ENTRY("%p %p %p %p %p", parent, xmlcode, xmlname, xmlmandatory, xmlvendor);
	CHECK_PARAMS( parent && xmlcode && xmlname );
	
	CHECK_FCT( xmltoint(xmlcode, &code) );
	
	if (xmlmandatory && !strcasecmp((char *)xmlmandatory, "must")) {
		flag |= AVP_FLAG_MANDATORY;
		fmask |= AVP_FLAG_MANDATORY;
	}
	
	if (xmlvendor) {
		CHECK_FCT( xmltoint(xmlvendor, &vendor) );
		if (vendor)
			flag |= AVP_FLAG_VENDOR;
		fmask |= AVP_FLAG_VENDOR;
	}
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_avp)) );
	memset(new, 0, sizeof(struct t_avp));
	fd_list_init(&new->chain, NULL);
	new->code = code;
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	new->flags = flag;
	new->fmask = fmask;
	new->vendor= vendor; 
	fd_list_init(&new->type, NULL);
	fd_list_init(&new->enums, NULL);
	fd_list_init(&new->grouped_fixed, NULL);
	fd_list_init(&new->grouped_required, NULL);
	fd_list_init(&new->grouped_optional, NULL);
	
	fd_list_insert_before(parent, &new->chain);
	
	*ret = new;
	
	return 0;
}

static void dump_avp(struct t_avp * a) {
	struct fd_list * li;
	fd_log_debug("  AVP %d %s%s: %s\n", a->code, 
		a->fmask & AVP_FLAG_MANDATORY ? ( a->flags & AVP_FLAG_MANDATORY ? "[M=1]" : "[M=0]") : "", 
		a->fmask & AVP_FLAG_VENDOR ? ( a->flags & AVP_FLAG_VENDOR ? "[V=1]" : "[V=0]") : "", 
		a->name);
	if (a->fmask & AVP_FLAG_VENDOR)
		fd_log_debug("    vendor: %d\n", a->vendor);
	for (li = a->type.next; li != &a->type; li = li->next)
		dump_avptype((struct t_avptype *)li);
	for (li = a->enums.next; li != &a->enums; li = li->next)
		dump_enum((struct t_enum *)li);
	for (li = a->grouped_fixed.next; li != &a->grouped_fixed; li = li->next)
		dump_rule((struct t_rule *)li, "    Grouped, fixed    AVP:");
	for (li = a->grouped_required.next; li != &a->grouped_required; li = li->next)
		dump_rule((struct t_rule *)li, "    Grouped, required AVP:");
	for (li = a->grouped_optional.next; li != &a->grouped_optional; li = li->next)
		dump_rule((struct t_rule *)li, "    Grouped, optional AVP:");
}

static void del_avp_contents(struct t_avp * a) {
	TRACE_ENTRY("%p", a);
	free(a->name);
	while (!FD_IS_LIST_EMPTY(&a->type)) {
		struct fd_list * li = a->type.next;
		fd_list_unlink(li);
		del_avptype_contents((struct t_avptype *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->enums)) {
		struct fd_list * li = a->enums.next;
		fd_list_unlink(li);
		del_enum_contents((struct t_enum *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->grouped_fixed)) {
		struct fd_list * li = a->grouped_fixed.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->grouped_required)) {
		struct fd_list * li = a->grouped_required.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->grouped_optional)) {
		struct fd_list * li = a->grouped_optional.next;
		fd_list_unlink(li);
		del_rule_contents((struct t_rule *)li);
		free(li);
	}
}


/* APPLICATION */
struct t_appl {
	struct fd_list 	chain; /* link in the t_dictionary->base_and_applications, the sentinel corresponds to "base" */
	uint32_t	id;
	uint8_t *	name;
	struct fd_list  commands; /* list of t_cmd */
	struct fd_list	types;    /* list of t_typedefn */
	struct fd_list  avps;     /* list of t_avp */
};

static int new_appl(struct fd_list * parent, xmlChar * xmlid, xmlChar * xmlname /* We ignore the URI */, struct t_appl **ret) {
	struct t_appl * new;
	uint32_t id = 0;
	
	TRACE_ENTRY("%p %p %p", parent, xmlid, xmlname);
	CHECK_PARAMS( parent && xmlid && xmlname );
	
	CHECK_FCT( xmltoint(xmlid, &id) );
	
	CHECK_MALLOC( new = malloc(sizeof(struct t_appl)) );
	memset(new, 0, sizeof(struct t_appl));
	fd_list_init(&new->chain, NULL);
	new->id = id;
	CHECK_MALLOC( new->name = (uint8_t *)strdup((char *)xmlname) );
	
	fd_list_init(&new->commands, NULL);
	fd_list_init(&new->types, NULL);
	fd_list_init(&new->avps, NULL);
	
	fd_list_insert_before(parent, &new->chain);
	
	*ret = new;
	
	return 0;
}

static void dump_appl(struct t_appl * a) {
	struct fd_list * li;
	fd_log_debug(" Application %d: %s\n", a->id, a->name);
	for (li = a->commands.next; li != &a->commands; li = li->next)
		dump_cmd((struct t_cmd *)li);
	for (li = a->types.next; li != &a->types; li = li->next)
		dump_type((struct t_typedefn *)li);
	for (li = a->avps.next; li != &a->avps; li = li->next)
		dump_avp((struct t_avp *)li);
}

static void del_appl_contents(struct t_appl * a) {
	TRACE_ENTRY("%p", a);
	free(a->name);
	while (!FD_IS_LIST_EMPTY(&a->commands)) {
		struct fd_list * li = a->commands.next;
		fd_list_unlink(li);
		del_cmd_contents((struct t_cmd *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->types)) {
		struct fd_list * li = a->types.next;
		fd_list_unlink(li);
		del_type_contents((struct t_typedefn *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&a->avps)) {
		struct fd_list * li = a->avps.next;
		fd_list_unlink(li);
		del_avp_contents((struct t_avp *)li);
		free(li);
	}
}

/* DICTIONARY */	
struct t_dictionary {
	struct fd_list  vendors;
	struct t_appl   base_and_applications;
};

static void dump_dict(struct t_dictionary * d) {
	struct fd_list * li;
	for (li = d->vendors.next; li != &d->vendors; li = li->next)
		dump_vendor((struct t_vend *)li);
	dump_appl(&d->base_and_applications);
	for (li = d->base_and_applications.chain.next; li != &d->base_and_applications.chain; li = li->next)
		dump_appl((struct t_appl *)li);
}

static void del_dict_contents(struct t_dictionary * d) {
	TRACE_ENTRY("%p", d);
	while (!FD_IS_LIST_EMPTY(&d->vendors)) {
		struct fd_list * li = d->vendors.next;
		fd_list_unlink(li);
		del_vendor_contents((struct t_vend *)li);
		free(li);
	}
	while (!FD_IS_LIST_EMPTY(&d->base_and_applications.chain)) {
		struct fd_list * li = d->base_and_applications.chain.next;
		fd_list_unlink(li);
		del_appl_contents((struct t_appl *)li);
		free(li);
	}
	d->base_and_applications.name = NULL;
	del_appl_contents(&d->base_and_applications);
}

/*********************************************/
	
/* The states for the SAX parser, corresponding roughly to the expected structure of the XML file. 
We use the states mostly to validate the XML file. */
enum state {
	START = 1, /* In "dictionary" */
	 IN_VENDOR,
	 IN_APPLICATION,        /* note that "base" is equivalent to "application" for our state machine */
	  IN_COMMAND,
	   IN_REQRULES,
	    IN_REQRULES_FIXED,
	    IN_REQRULES_REQUIRED,
	    IN_REQRULES_OPTIONAL,
	   IN_ANSRULES,
	    IN_ANSRULES_FIXED,
	    IN_ANSRULES_REQUIRED,
	    IN_ANSRULES_OPTIONAL,
	  IN_TYPEDEFN,
	  IN_AVP,
	   IN_AVP_TYPE,
	   IN_AVP_ENUM,
	   IN_AVP_GROUPED,
	    IN_AVP_GROUPED_FIXED,
	    IN_AVP_GROUPED_REQUIRED,
	    IN_AVP_GROUPED_OPTIONAL
};


/* The context passed to the SAX parser */
struct parser_ctx {
	enum state 		state; 	     /* the current state */
	int        		error_depth; /* if non 0, we are in an unexpected element, wait until the count goes back to 0 to resume normal parsing. */
	struct t_dictionary 	dict;        /* The dictionary being built */
	struct t_appl *		cur_app;
	struct t_cmd  *		cur_cmd;
	struct t_avp  *		cur_avp;
	char * 			xmlfilename; /* Name of the file, for error messages */
};

/* Find an attribute with given name in the list */
static void get_attr(const xmlChar ** atts_array, const char * attr_name, xmlChar ** attr_val) {
	int i;
	*attr_val = NULL;
	if (atts_array == NULL)
		return;
	for (i=0; atts_array[i] != NULL; i+=2) {
		if (!strcasecmp((char *)atts_array[i], attr_name)) {
			/* found */
			*attr_val = (xmlChar *)atts_array[i+1];
			return;
		}
	}
	/* not found */
	return;
}

/* The following macro avoids duplicating a lot of code in the state machine */
#define ADD_RULE( _parent_list ) { 				\
	xmlChar *xname, *xmin, *xmax;				\
	/* We are expecting an <avprule> tag at this point */	\
	if (strcasecmp((char *)name, "avprule"))		\
		goto xml_tree_error;				\
	/* Search the expected attributes */			\
	get_attr(atts, "name", &xname);				\
	get_attr(atts, "maximum", &xmax);			\
	get_attr(atts, "minimum", &xmin);			\
	/* Check the mandatory name is here */			\
	CHECK_PARAMS_DO(xname, 					\
		{ TRACE_DEBUG(INFO, "Invalid 'avprule' tag found without 'name' attribute."); goto xml_tree_error; } );	\
	/* Create the rule and add into the parent list */	\
	CHECK_FCT_DO( new_rule((_parent_list), xname, xmax, xmin),\
		{ TRACE_DEBUG(INFO, "An error occurred while parsing an avprule tag. Entry ignored."); goto xml_tree_error; } ); \
	/* Done. we don't change the state */			\
}
	
	
/* The function called on each XML element start tag (startElementSAXFunc) */
static void SAXstartelem (void * ctx, const xmlChar * name, const xmlChar ** atts)
{
	struct parser_ctx * data = ctx;
	TRACE_ENTRY("%p %p %p", ctx, name, atts);
	CHECK_PARAMS_DO( ctx && name, { return; } );
	
	TRACE_DEBUG(CALL, "Tag: <%s>", (char *)name);
	
	if (data->error_depth) /* we are in an unknown element, just skip until it is closed */
		goto xml_tree_error;
	
	switch (data->state) {
		case 0: /* we are just starting. We only expect a <dictionary> tag, reject anything else. */
			if (strcasecmp((char *)name, "dictionary"))
				goto xml_tree_error;
			
			data->state = START;
			break;
			
		case START: 
			/* We are in <dictionary> 
				Valid tags are: <vendor>, <base>, <application> */
			if (!strcasecmp((char *)name, "vendor")) {
				xmlChar *xid, *xname;
				
				get_attr(atts, "id", &xid);
				get_attr(atts, "name", &xname);
				
				/* id and name are required */
				CHECK_PARAMS_DO(xid && xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'vendor' tag found without 'id' or 'name' attribute."); goto xml_tree_error; } );
				
				
				CHECK_FCT_DO( new_vendor(&data->dict.vendors, xid, xname),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing a vendor tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_VENDOR;
				break;
			}
			
			if (!strcasecmp((char *)name, "base")) {
				/* we don't care for the 'uri' attribute */
				data->cur_app = &data->dict.base_and_applications;
				data->state = IN_APPLICATION;
				break;
			}
			
			if (!strcasecmp((char *)name, "application")) {
				/* we don't care for the 'uri' attribute */
				xmlChar *xid, *xname;
				char buf[50];
				
				get_attr(atts, "id", &xid);
				get_attr(atts, "name", &xname);
				
				CHECK_PARAMS_DO(xid, 
					{ TRACE_DEBUG(INFO, "Invalid 'application' tag found without 'id' attribute."); goto xml_tree_error; } );
				
				/* Name is optional, if not provided we create a name */
				if (!xname) {
					snprintf(buf, sizeof(buf), "Application %s", xid);
					xname = (xmlChar *)buf;
				}
				
				CHECK_FCT_DO( new_appl(&data->dict.base_and_applications.chain, xid, xname, &data->cur_app),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing an application tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_APPLICATION;
				break;
			}
			
			/* Other tags are errors */
			goto xml_tree_error;
			
			
		case IN_VENDOR: /* nothing is allowed inside <vendor> */
			goto xml_tree_error;
				
		case IN_APPLICATION: 
			/* We are in <base> or <application>
				Valid tags are: <command>, <typedefn>, <avp> */
			if (!strcasecmp((char *)name, "command")) {
				/* we don't care for the 'vendor-id' attribute. */
				xmlChar *xcode, *xname, *xpbit;
				
				get_attr(atts, "code", &xcode);
				get_attr(atts, "name", &xname);
				get_attr(atts, "pbit", &xpbit);
				
				/* code and name are required */
				CHECK_PARAMS_DO(xcode && xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'command' tag found without 'code' or 'name' attribute."); goto xml_tree_error; } );
				
				CHECK_FCT_DO( new_cmd( &data->cur_app->commands, xcode, xname, xpbit, &data->cur_cmd),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing a command tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_COMMAND;
				break;
			}
			
			if (!strcasecmp((char *)name, "typedefn")) {
				/* we don't care for the 'description' attribute. */
				xmlChar *xname, *xparent;
				
				get_attr(atts, "type-name", &xname);
				get_attr(atts, "type-parent", &xparent);
				
				/* name is required */
				CHECK_PARAMS_DO(xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'typedefn' tag found without 'name' attribute."); goto xml_tree_error; } );
				
				CHECK_FCT_DO( new_type( &data->cur_app->types, xname, xparent),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing a typedefn tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_TYPEDEFN;
				break;
			}
			
			if (!strcasecmp((char *)name, "avp")) {
				/* we don't care for the description, may-encrypt, and protected attributes */
				xmlChar *xname, *xcode, *xmandatory, *xvendor;
				
				get_attr(atts, "name", &xname);
				get_attr(atts, "code", &xcode);
				get_attr(atts, "mandatory", &xmandatory);
				get_attr(atts, "vendor-id", &xvendor);
				
				/* code and name are required */
				CHECK_PARAMS_DO(xcode && xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'avp' tag found without 'code' or 'name' attribute."); goto xml_tree_error; } );
				
				CHECK_FCT_DO( new_avp(&data->cur_app->avps, xcode, xname, xmandatory, xvendor, &data->cur_avp),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing an avp tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_AVP;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
		
			
		case IN_COMMAND: 
			/* We are in <command>
				Valid tags are: <requestrules>, <answerrules> */
			if (!strcasecmp((char *)name, "requestrules")) {
				data->state = IN_REQRULES;
				break;
			}
			if (!strcasecmp((char *)name, "answerrules")) {
				data->state = IN_ANSRULES;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
		
		case IN_REQRULES: 
			/* We are in <requestrules>
				Valid tags are: <fixed>, <required>, <optional> */
			if (!strcasecmp((char *)name, "fixed")) {
				data->state = IN_REQRULES_FIXED;
				break;
			}
			if (!strcasecmp((char *)name, "required")) {
				data->state = IN_REQRULES_REQUIRED;
				break;
			}
			if (!strcasecmp((char *)name, "optional")) {
				data->state = IN_REQRULES_OPTIONAL;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
		
		case IN_ANSRULES: 
			/* We are in <answerrules>
				Valid tags are: <fixed>, <required>, <optional> */
			if (!strcasecmp((char *)name, "fixed")) {
				data->state = IN_ANSRULES_FIXED;
				break;
			}
			if (!strcasecmp((char *)name, "required")) {
				data->state = IN_ANSRULES_REQUIRED;
				break;
			}
			if (!strcasecmp((char *)name, "optional")) {
				data->state = IN_ANSRULES_OPTIONAL;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
			
		case IN_REQRULES_FIXED:
			/* We are in <command><answerrules><fixed>
				Valid tags are: <avprule> */
			ADD_RULE( &data->cur_cmd->reqrules_fixed );
			break;
		case IN_REQRULES_REQUIRED:
			ADD_RULE( &data->cur_cmd->reqrules_required );
			break;
		case IN_REQRULES_OPTIONAL:
			ADD_RULE( &data->cur_cmd->reqrules_optional );
			break;
		case IN_ANSRULES_FIXED:
			ADD_RULE( &data->cur_cmd->ansrules_fixed );
			break;
		case IN_ANSRULES_REQUIRED:
			ADD_RULE( &data->cur_cmd->ansrules_required );
			break;
		case IN_ANSRULES_OPTIONAL:
			ADD_RULE( &data->cur_cmd->ansrules_optional );
			break;
			
		
		case IN_TYPEDEFN: /* nothing is allowed inside <typedefn> */
			goto xml_tree_error;
				
		
		case IN_AVP: 
			/* We are in <avp>
				Valid tags are: <type>, <enum>, <grouped> */
			if (!strcasecmp((char *)name, "type")) {
				xmlChar *xname;
				
				get_attr(atts, "type-name", &xname);
				
				/* name is required */
				CHECK_PARAMS_DO(xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'type' tag found without 'name' attribute."); goto xml_tree_error; } );
				
				CHECK_FCT_DO( new_avptype(&data->cur_avp->type, xname),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing a type tag. Entry ignored."); goto xml_tree_error; } )
				
				data->state = IN_AVP_TYPE;
				break;
			}
			if (!strcasecmp((char *)name, "enum")) {
				xmlChar *xcode, *xname;
				
				get_attr(atts, "code", &xcode);
				get_attr(atts, "name", &xname);
				
				/* code and name are required */
				CHECK_PARAMS_DO(xcode && xname, 
					{ TRACE_DEBUG(INFO, "Invalid 'enum' tag found without 'code' or 'name' attribute."); goto xml_tree_error; } );
				
				CHECK_FCT_DO( new_enum(&data->cur_avp->enums, xcode, xname),
					{ TRACE_DEBUG(INFO, "An error occurred while parsing a command tag. Entry ignored."); goto xml_tree_error; } )
			
				data->state = IN_AVP_ENUM;
				break;
			}
			if (!strcasecmp((char *)name, "grouped")) {
				/* no attribute for this one */
				data->state = IN_AVP_GROUPED;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
		
		case IN_AVP_TYPE: /* nothing is allowed inside <type> */
			goto xml_tree_error;
			
		case IN_AVP_ENUM: /* nothing is allowed inside <enum> */
			goto xml_tree_error;
			
		case IN_AVP_GROUPED: 
			/* We are in <avp><grouped>
				Valid tags are: <fixed>, <required>, <optional> */
			if (!strcasecmp((char *)name, "fixed")) {
				data->state = IN_AVP_GROUPED_FIXED;
				break;
			}
			if (!strcasecmp((char *)name, "required")) {
				data->state = IN_AVP_GROUPED_REQUIRED;
				break;
			}
			if (!strcasecmp((char *)name, "optional")) {
				data->state = IN_AVP_GROUPED_OPTIONAL;
				break;
			}
			/* Other tags are errors */
			goto xml_tree_error;
			
		case IN_AVP_GROUPED_FIXED:
			/* We are in <avp><grouped><fixed>
				Valid tags are: <avprule> */
			ADD_RULE( &data->cur_avp->grouped_fixed );
			break;
		case IN_AVP_GROUPED_REQUIRED:
			ADD_RULE( &data->cur_avp->grouped_required );
			break;
		case IN_AVP_GROUPED_OPTIONAL:
			ADD_RULE( &data->cur_avp->grouped_optional );
			break;
		
			
		default:
			TRACE_DEBUG(INFO, "Internal parsing error, unexpected state %d.", data->state);
	}
	
	return;
	
xml_tree_error:
	if (!data->error_depth) {
		TRACE_DEBUG(INFO, "Unexpected XML element found: '%s'. Ignoring...", name);
	}
	data->error_depth += 1;
	return;
}

/* The function called on each XML element end tag (endElementSAXFunc) */
static void SAXendelem (void * ctx, const xmlChar * name)
{
	struct parser_ctx * data = ctx;
	TRACE_ENTRY("%p %p", ctx, name);
	CHECK_PARAMS_DO( ctx && name, { return; } );
	
	TRACE_DEBUG(CALL, "Tag: </%s>", (char *)name);
	
	if (data->error_depth) {
		/* we are recovering from an erroneous element */
		data->error_depth -= 1;
		return;
	}
	
	switch (data->state) {
		case 0: 
			goto state_machine_error;
			
		case START:
			if (strcasecmp((char *)name, "dictionary"))
				goto state_machine_error;
			
			data->state = 0;
			break;
		
		case IN_VENDOR:
			if (strcasecmp((char *)name, "vendor"))
				goto state_machine_error;
			
			data->state = START;
			break;
		
		case IN_APPLICATION:
			if (strcasecmp((char *)name, "base") && strcasecmp((char *)name, "application"))
				goto state_machine_error;
			
			data->cur_app = NULL;
			data->state = START;
			break;
		
		case IN_COMMAND:
			if (strcasecmp((char *)name, "command"))
				goto state_machine_error;
			
			data->cur_cmd = NULL;
			data->state = IN_APPLICATION;
			break;
			
		case IN_REQRULES:
			if (strcasecmp((char *)name, "requestrules"))
				goto state_machine_error;
			
			data->state = IN_COMMAND;
			break;
		
		case IN_REQRULES_FIXED:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "fixed"))
				goto state_machine_error;
			data->state = IN_REQRULES;
			break;
		case IN_REQRULES_REQUIRED:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "required"))
				goto state_machine_error;
			data->state = IN_REQRULES;
			break;
		case IN_REQRULES_OPTIONAL:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "optional"))
				goto state_machine_error;
			data->state = IN_REQRULES;
			break;
			
		case IN_ANSRULES: 
			if (strcasecmp((char *)name, "answerrules"))
				goto state_machine_error;
			
			data->state = IN_COMMAND;
			break;			
		case IN_ANSRULES_FIXED:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "fixed"))
				goto state_machine_error;
			data->state = IN_ANSRULES;
			break;
		case IN_ANSRULES_REQUIRED:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "required"))
				goto state_machine_error;
			data->state = IN_ANSRULES;
			break;
		case IN_ANSRULES_OPTIONAL:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "optional"))
				goto state_machine_error;
			data->state = IN_ANSRULES;
			break;
		
		
		case IN_TYPEDEFN:
			if (strcasecmp((char *)name, "typedefn"))
				goto state_machine_error;
			
			data->state = IN_APPLICATION;
			break;
		
		case IN_AVP: 
			if (strcasecmp((char *)name, "avp"))
				goto state_machine_error;
			
			data->cur_avp = NULL;
			data->state = IN_APPLICATION;
			break;
		
		case IN_AVP_TYPE:
			if (strcasecmp((char *)name, "type"))
				goto state_machine_error;
			
			data->state = IN_AVP;
			break;
		
		case IN_AVP_ENUM:
			if (strcasecmp((char *)name, "enum"))
				goto state_machine_error;
			
			data->state = IN_AVP;
			break;
		
		case IN_AVP_GROUPED:
			if (strcasecmp((char *)name, "grouped"))
				goto state_machine_error;
			
			data->state = IN_AVP;
			break;
		
		case IN_AVP_GROUPED_FIXED:
			if (!strcasecmp((char *)name, "avprule"))
				/* we don't have a special state for these, just ignore */
				return;
			if (strcasecmp((char *)name, "fixed"))
				goto state_machine_error;
			data->state = IN_AVP_GROUPED;
			break;
		case IN_AVP_GROUPED_REQUIRED:
			if (!strcasecmp((char *)name, "avprule"))
				return;
			if (strcasecmp((char *)name, "required"))
				goto state_machine_error;
			data->state = IN_AVP_GROUPED;
			break;
		case IN_AVP_GROUPED_OPTIONAL:
			if (!strcasecmp((char *)name, "avprule"))
				return;
			if (strcasecmp((char *)name, "optional"))
				goto state_machine_error;
			data->state = IN_AVP_GROUPED;
			break;
			
		default:
			TRACE_DEBUG(INFO, "Internal parsing error, unexpected state %d.", data->state);
	}
	
	return;
	
state_machine_error:
	TRACE_DEBUG(INFO, "Internal parsing error, ignored [state %d, closing tag '%s'].", data->state, name);
	return;
}

/* The SAX parser sends a warning, error, fatalerror 
static void SAXwarning (void * ctx, const char * msg, ...)
{

}
static void SAXerror (void * ctx, const char * msg, ...)
{

}
static void SAXfatal (void * ctx, const char * msg, ...)
{

}
*/

/*********************************************/
 /* 2nd pass: from memory to fD dictionary */

static int dict_to_fD(struct t_dictionary * dict, int * nb_created)
{
	TODO("Parse the dict tree, add objects in the fD dictionary, update count");
	return ENOTSUP;
}





/*********************************************/

int dict_lxml_parse(char * xmlfilename)
{
	xmlSAXHandler handler;
	struct parser_ctx data;
	int ret;

	TRACE_ENTRY("%p", xmlfilename);
	
	CHECK_PARAMS_DO(xmlfilename, { return -1; } );
	
	TRACE_DEBUG(FULL, "Parsing next XML file: %s...", xmlfilename);
	
	/* Initialize the parser */
	memset(&handler, 0, sizeof(handler));
	handler.startElement = SAXstartelem;
	handler.endElement   = SAXendelem;
	
	/* Initialize the data */
	memset(&data, 0, sizeof(data));
	fd_list_init( &data.dict.vendors, NULL );
	fd_list_init( &data.dict.base_and_applications.chain, NULL );
	data.dict.base_and_applications.name = (uint8_t *)"Diameter Base Protocol";
	fd_list_init( &data.dict.base_and_applications.commands, NULL );
	fd_list_init( &data.dict.base_and_applications.types, NULL );
	fd_list_init( &data.dict.base_and_applications.avps, NULL );
	data.xmlfilename = xmlfilename;
	
	/* Parse the file */
	ret = xmlSAXUserParseFile(&handler, &data, xmlfilename);
	if (ret < 0) {
		TRACE_DEBUG(INFO, "An error occurred while parsing %s, abording.", xmlfilename);
		del_dict_contents(&data.dict);
		return -1;
	}
	
	TRACE_DEBUG(FULL, "XML Parsed successfully");
	if (TRACE_BOOL(ANNOYING)) {
		dump_dict(&data.dict);
	}
	
	/* Now, convert all the objects from the temporary tree into the freeDiameter dictionary */
	ret = 0;
	CHECK_FCT_DO( dict_to_fD(&data.dict, &ret), { del_dict_contents(&data.dict); return -1; } );
	
	/* Done */
	del_dict_contents(&data.dict);
	
	return ret;
}
