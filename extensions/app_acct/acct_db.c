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

/* Database interface module */

#include "app_acct.h"
#include <libpq-fe.h>

const char * diam2db_types_mapping[AVP_TYPE_MAX + 1] = {
	"" 		/* AVP_TYPE_GROUPED */,
	"bytea" 	/* AVP_TYPE_OCTETSTRING */,
	"integer" 	/* AVP_TYPE_INTEGER32 */,
	"bigint" 	/* AVP_TYPE_INTEGER64 */,
	"integer" 	/* AVP_TYPE_UNSIGNED32 + cast */,
	"bigint" 	/* AVP_TYPE_UNSIGNED64 + cast */,
	"real" 		/* AVP_TYPE_FLOAT32 */,
	"double precision" /* AVP_TYPE_FLOAT64 */
};

static const char * stmt = "acct_db_stmt";
static PGconn *conn = NULL;

int acct_db_init(void)
{
	struct acct_record_list emptyrecords;
	struct fd_list * li;
	char * sql=NULL;   /* The buffer that will contain the SQL query */
	size_t sql_allocd = 0; /* The malloc'd size of the buffer */
	size_t sql_offset = 0; /* The actual data already written in this buffer */
	size_t p;
	int idx = 0;
	PGresult * res;
	#define REALLOC_SIZE	1024
	
	TRACE_ENTRY();
	CHECK_PARAMS( acct_config && acct_config->conninfo && acct_config->tablename ); 
	
	/* Use the information from acct_config to create the connection and prepare the query */
	conn = PQconnectdb(acct_config->conninfo);
	
	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK) {
		fd_log_debug("Connection to database failed: %s\n", PQerrorMessage(conn));
		acct_db_free();
		return EINVAL;
	} else {
		TRACE_DEBUG(INFO, "Connection to database successfull: user:%s, db:%s, host:%s.", PQuser(conn), PQdb(conn), PQhost(conn));
	}
	
	/* Now, prepare the request object */
	
	/* First, we build the list of AVP we will insert in the database */
	CHECK_FCT( acct_rec_prepare(&emptyrecords) );
	
	/* Now, prepare the text of the request */
	CHECK_MALLOC(sql = malloc(REALLOC_SIZE));
	sql_allocd = REALLOC_SIZE;
	
	/* This macro hides the details of extending the buffer on each sprintf... */
	#define ADD_EXTEND(args...) {									\
		size_t p;										\
		int loop = 0;										\
		do {											\
			p = snprintf(sql + sql_offset, sql_allocd - sql_offset, ##args);		\
			if (p >= sql_allocd - sql_offset) {						\
				/* Too short, extend the buffer and start again */			\
				CHECK_MALLOC( sql = realloc(sql, sql_allocd + REALLOC_SIZE) );		\
				sql_allocd += REALLOC_SIZE;						\
				loop++;									\
				ASSERT(loop < 100); /* detect infinite loops */				\
				continue;								\
			}										\
			sql_offset += p;								\
			break;										\
		} while (1);										\
	}
	
	/* This macro allows to add a value in the SQL buffer while escaping in properly */
	#define ADD_ESCAPE(str) {									\
		char * __s = (char *)str;								\
		/* Check we have at least twice the size available +1 */				\
		size_t p = strlen(__s);									\
													\
		while (sql_allocd - sql_offset < 2 * p + 1) {						\
			/* Too short, extend the buffer */						\
			CHECK_MALLOC( sql = realloc(sql, sql_allocd + REALLOC_SIZE) );			\
			sql_allocd += REALLOC_SIZE;							\
		}											\
													\
		/* Now add the escaped string */							\
		p = PQescapeStringConn(conn, sql+sql_offset, __s, p, NULL);				\
		sql_offset += p;									\
	}
	
	/* INSERT INTO table (tsfield, field1, field2, ...) VALUES (now, $1::bytea, $2::integer, ...) */
	ADD_EXTEND("INSERT INTO %s (", acct_config->tablename);
	
	if (acct_config->tsfield) {
		ADD_EXTEND("\"");
		ADD_ESCAPE(acct_config->tsfield);
		ADD_EXTEND("\", ");
	}
	
	for (li = emptyrecords.all.next; li != &emptyrecords.all; li = li->next) {
		struct acct_record_item * i = (struct acct_record_item *)(li->o);
		ADD_EXTEND("\"");
		ADD_ESCAPE(i->param->field?:i->param->avpname);
		if (i->index) {
			ADD_EXTEND("%d", i->index);
		}
		if (li->next != &emptyrecords.all) {
			ADD_EXTEND("\", ");
		}
	}
	
	ADD_EXTEND("\") VALUES (");
	
	if (acct_config->tsfield) {
		ADD_EXTEND("'now', ");
	}
	
	for (li = emptyrecords.all.next; li != &emptyrecords.all; li = li->next) {
		struct acct_record_item * i = (struct acct_record_item *)(li->o);
		idx += 1;
		ADD_EXTEND("$%d::%s", idx, diam2db_types_mapping[i->param->avptype]);
		
		if (li->next != &emptyrecords.all) {
			ADD_EXTEND(", ");
		}
	}
	
	ADD_EXTEND(");");
	
	TRACE_DEBUG(FULL, "Preparing the following SQL statement:\n%s\n", sql);
	res = PQprepare(conn, stmt, sql, emptyrecords.nball, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		TRACE_DEBUG(INFO, "Preparing statement '%s' failed: %s",
			sql, PQerrorMessage(conn));
		PQclear(res);
		return EINVAL;
        }
	PQclear(res);
	
	

	return 0;
}

void acct_db_free(void)
{
	if (conn)
		PQfinish(conn);
	conn = NULL;
}

int acct_db_insert(struct acct_record_list * records)
{
	return ENOTSUP;
}


