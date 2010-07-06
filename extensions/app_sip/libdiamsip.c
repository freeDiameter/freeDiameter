/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Alexandre Westfahl <awestfahl@freediameter.net>						 *
*													 *
* Copyright (c) 2010, Alexandre Westfahl, Teraoka Laboratory (Keio University), and the WIDE Project. 	 *		
*													 *
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
* * Neither the name of the Teraoka Laboratory nor the 							 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of Teraoka Laboratory 						 *
*   													 *
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
#include <mysql.h>
#include "diamsip.h"

MYSQL *conn;
/*
void calculate_nonce(u8 * nonce)
{

	nonce="lkgbsljhsdjdsgj";
	return;
}*/

void clear_digest(char * digest, char * readable_digest, int digestlength)
{
	int i=0;
	for(i=0;i<digestlength * 2;i++)
		sprintf(&readable_digest[2 * i], "%2.2hhx", digest[i]);
	readable_digest[2 * digestlength]='\0';
	
	return;
}

// You must create a table like this "char  clearDigest[DIGEST_LEN*2+1];"
void calc_md5(char *clearDigest, char * data)
{
	gcry_md_hd_t md5;
	uint8_t * binDigest=NULL;
	
	CHECK_MALLOC_DO(binDigest=malloc(DIGEST_LEN),return);
	
	gcry_md_open(&md5,GCRY_MD_MD5, 0); 
	gcry_md_write(md5, (char *)data, sizeof(data));
	memcpy(binDigest, gcry_md_read(md5,  GCRY_MD_MD5),gcry_md_get_algo_dlen(GCRY_MD_MD5));
	gcry_md_close(md5);
	
	clear_digest(binDigest, clearDigest, DIGEST_LEN);
	free(binDigest);
	return;
}


/* Search a given AVP model in an AVP (extracted from libfreediameter/message.c ) */
int fd_avp_search_avp ( struct avp * groupedavp, struct dict_object * what, struct avp ** avp )
{
	struct avp * nextavp;
	struct avp_hdr * nextavphdr;
	struct dict_avp_data 	dictdata;
	enum dict_object_type 	dicttype;
	
	TRACE_ENTRY("%p %p %p", groupedavp, what, avp);
	
	CHECK_FCT(  fd_dict_getval(what, &dictdata)  );
	
	// Loop only in the group AVP 
	CHECK_FCT(  fd_msg_browse(groupedavp, MSG_BRW_FIRST_CHILD, (void *)&nextavp, NULL)  );
	CHECK_FCT( fd_msg_avp_hdr( nextavp, &nextavphdr )  );
	
	while (nextavphdr) {
		
		if ( (nextavphdr->avp_code   == dictdata.avp_code) && (nextavphdr->avp_vendor == dictdata.avp_vendor) ) // always 0 if no Vendor flag
		{
			break;
		}
		
		// Otherwise move to next AVP in the grouped AVP 
		CHECK_FCT( fd_msg_browse(nextavp, MSG_BRW_NEXT, (void *)&nextavp, NULL) );
		
		if(nextavp!=NULL)
		{
			CHECK_FCT( fd_msg_avp_hdr( nextavp, &nextavphdr )  );
		}
		else
			nextavphdr=NULL;
	}
	if (avp)
		*avp = nextavp;
	
	if (avp && nextavp) {
		struct dictionary * dict;
		CHECK_FCT( fd_dict_getdict( what, &dict) );
		CHECK_FCT_DO( fd_msg_parse_dict( nextavp, dict, NULL ),  );
	}
	
	if (avp || nextavp)
		return 0;
	else
		return ENOENT;
}
struct avp_hdr *walk_digest(struct avp *avp, int avp_code)
{
	struct avp_hdr *temphdr=NULL;
	CHECK_FCT_DO(fd_msg_browse ( avp, MSG_BRW_WALK, &avp, NULL),0 );
	
	while(avp!=NULL)
	{
		
		CHECK_FCT_DO( fd_msg_avp_hdr( avp,&temphdr ),0);

		if(temphdr->avp_code==avp_code)
		{
			//We found the AVP so we set avp to NULL to exit the loop
			avp=NULL;
			return temphdr;
			
		}
		else if(temphdr->avp_code==380)//SIP-Authorization AVP
		{
			//We didn't found the AVP but we finished browsing the Authentication AVP
			avp=NULL;
			temphdr=NULL;
			
			return temphdr;
		}
		else
		{
			CHECK_FCT_DO(fd_msg_browse ( avp, MSG_BRW_WALK, &avp, NULL),0 );
			temphdr=NULL;
			
		}
	}
	
	return temphdr;
}

int start_mysql_connection()
{
	conn = mysql_init(NULL);
	
	mysql_options(conn, MYSQL_OPT_RECONNECT, "true");
	
	if (!mysql_real_connect(conn, as_conf->mysql_server,as_conf->mysql_login, as_conf->mysql_password, as_conf->mysql_database, as_conf->mysql_port, NULL, 0)) 
	{//TODO: display error from mysql
		TRACE_DEBUG(INFO,"Unable to connect to database (%s) with login:%s",as_conf->mysql_database,as_conf->mysql_login);
		return 1;
	}
	return 0;
	
}

//You must free ""result"" after using this function
void request_mysql(char *query)
{
	//We check if the connection is still up
	mysql_ping(conn);
	
	if (mysql_query(conn, query)) 
	{
		TRACE_DEBUG(INFO,"Query %s failed", query);
		
	}
	
}

void close_mysql_connection()
{
	mysql_close(conn);
	
}
/*
void nonce_add_element(char * nonce)
{
	noncechain *newelt=malloc(sizeof(noncechain));
	
	newelt->nonce=nonce;
	
	newelt->timestamp=(int)time(NULL);
	newelt->next=NULL;
	
	if(listnonce==NULL)
	{
		listnonce=newelt;
	}
	else
	{
		noncechain* temp=listnonce;
		
		while(temp->next != NULL)
		{
			if(temp->timestamp < ((int)time(NULL)-300))
			{
				listnonce=temp->next;
				free(temp);
				temp=listnonce;
			}
			temp = temp->next;
		}
		temp->next = newelt;
	}
	
}
void nonce_del_element(char * nonce)
{
	if(listnonce!=NULL)
	{
		noncechain *temp=listnonce, *tempbefore=NULL;
		
		if(listnonce->next==NULL && strcmp(listnonce->nonce,nonce)==0)
		{
			free(listnonce);
			listnonce=NULL;
			return;
		}
		while(temp->next != NULL)
		{
			if(strcmp(temp->nonce,nonce)==0)
			{
				if(tempbefore==NULL)
				{
					listnonce=temp->next;
					free(temp);
					return;
				}
				tempbefore->next=temp->next;
				free(temp);
				break;
			}
			tempbefore=temp;
			temp = temp->next;
		}
		
	}
	
}
int nonce_check_element(char * nonce)
{
	if(listnonce==NULL)
	{
		//Not found
		return 0;
	}
	else
	{
		noncechain* temp=listnonce;
		
		while(temp->next != NULL)
		{
			if(strcmp(temp->nonce,nonce)==0)
				return 1;
			else
				temp = temp->next;
		}
	}
	return 0;
}

void nonce_deletelistnonce()
{
	if(listnonce !=NULL)
	{
		noncechain* temp=listnonce;
	
		while(listnonce->next != NULL)
		{
			temp = listnonce->next;
		
			free(listnonce);
		
			listnonce=temp;
		}
		free(listnonce);
	}
}
*/
