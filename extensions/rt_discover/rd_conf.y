%debug
%define parse.error verbose
%define api.pure
%locations
%parse-param {struct rd_parse *parse}

%{
#include "rt_discover.h"
#include "rd_conf.tab.h"
#include <limits.h>

struct rd_parse {
	const char *path;
	struct fd_list *realms;
	struct rd_realm *current;
};

int rd_conflex(YYSTYPE *value, YYLTYPE *location);
int rd_conflex_destroy(void);
void rd_confrestart(FILE *input);
int yyparse(struct rd_parse *parse);

void yyerror(YYLTYPE *loc, struct rd_parse *parse, const char *message)
{
	LOG_E("%s:%d.%d: %s", parse->path, loc->first_line, loc->first_column, message);
}

static int rd_add_realm(struct rd_parse *parse, const char *name)
{
	struct fd_list *item;
	struct rd_realm *realm;

	if (!fd_os_is_valid_DiameterIdentity((os0_t)name, strlen(name)))
		return EINVAL;
	for (item = parse->realms->next; item != parse->realms; item = item->next) {
		realm = item->o;
		if (!strcasecmp(realm->name, name))
			return EEXIST;
	}
	CHECK_MALLOC(realm = calloc(1, sizeof(*realm)));
	if (!(realm->name = strdup(name))) {
		free(realm);
		return ENOMEM;
	}
	fd_list_init(&realm->chain, realm);
	fd_list_init(&realm->peers, NULL);
	fd_list_insert_before(parse->realms, &realm->chain);
	parse->current = realm;
	return 0;
}

static int rd_add_peer(struct rd_parse *parse, const char *identity)
{
	struct fd_list *item;
	struct rd_allowed_peer *peer;

	if (!fd_os_is_valid_DiameterIdentity((os0_t)identity, strlen(identity)))
		return EINVAL;
	for (item = parse->current->peers.next; item != &parse->current->peers; item = item->next) {
		peer = item->o;
		if (!strcasecmp(peer->identity, identity))
			return EEXIST;
	}
	CHECK_MALLOC(peer = calloc(1, sizeof(*peer)));
	if (!(peer->identity = strdup(identity))) {
		free(peer);
		return ENOMEM;
	}
	fd_list_init(&peer->chain, peer);
	fd_list_insert_before(&parse->current->peers, &peer->chain);
	return 0;
}

void rd_config_free(struct fd_list *realms)
{
	while (!FD_IS_LIST_EMPTY(realms)) {
		struct rd_realm *realm = realms->next->o;
		while (!FD_IS_LIST_EMPTY(&realm->peers)) {
			struct rd_allowed_peer *peer = realm->peers.next->o;
			fd_list_unlink(&peer->chain);
			free(peer->identity);
			free(peer);
		}
		fd_list_unlink(&realm->chain);
		free(realm->name);
		free((char *)realm->options.tls_priority);
		free((char *)realm->options.peer_realm);
		free(realm);
	}
}

int rd_config_load(const char *path, struct fd_list *realms)
{
	struct rd_parse parse;
	FILE *input;
	int ret, io_error;

	CHECK_PARAMS(path && realms);
	input = fopen(path, "r");
	if (!input) {
		ret = errno;
		LOG_E("Cannot open rt_discover configuration %s: %s", path, strerror(ret));
		return ret;
	}
	parse.path = path;
	parse.realms = realms;
	parse.current = NULL;
	rd_confrestart(input);
	ret = yyparse(&parse);
	io_error = ferror(input) ? EIO : 0;
	if (fclose(input) && !io_error)
		io_error = errno;
	CHECK_FCT(rd_conflex_destroy());
	if (io_error) {
		LOG_E("Cannot read rt_discover configuration %s: %s", path, strerror(io_error));
		rd_config_free(realms);
		return io_error;
	}
	if (ret) {
		rd_config_free(realms);
		return EINVAL;
	}
	return 0;
}
%}

%union { char *string; int integer; }
%token <string> QSTRING
%token <integer> INTEGER
%token REALM PEER LEX_ERROR
%token NOIP NOIP6 NOTCP NOSCTP PREFERTCP TCTIMER TWTIMER TLS_PRIO
%destructor { free($$); } QSTRING

%%
config:
	/* empty */
	| config realm
	;
realm:
	REALM '=' QSTRING {
		int ret = rd_add_realm(parse, $3);
		free($3);
		$3 = NULL;
		if (ret) {
			LOG_E("%s:%d.%d: Cannot add realm: %s",
			      parse->path, @3.first_line, @3.first_column, strerror(ret));
			YYERROR;
		}
	} '{' peers '}' ';' {
		if (FD_IS_LIST_EMPTY(&parse->current->peers)) {
			yyerror(&@1, parse, "Realm must specify at least one authorized peer");
			YYERROR;
		}
		parse->current = NULL;
	}
	;
peers:
	/* empty */
	| peers PEER '=' QSTRING ';' {
		int ret = rd_add_peer(parse, $4);
		free($4);
		$4 = NULL;
		if (ret) {
			LOG_E("%s:%d.%d: Cannot add peer identity: %s",
			      parse->path, @4.first_line, @4.first_column, strerror(ret));
			YYERROR;
		}
	}
	| peers NOIP ';' {
		if (fd_g_config->cnf_flags.no_ip6 ||
		    parse->current->options.ip_family == FD_DISCOVERY_IPV4) {
			yyerror(&@2, parse, "No_IP conflicts with No_IPv6");
			YYERROR;
		}
		parse->current->options.ip_family = FD_DISCOVERY_IPV6;
	}
	| peers NOIP6 ';' {
		if (fd_g_config->cnf_flags.no_ip4 ||
		    parse->current->options.ip_family == FD_DISCOVERY_IPV6) {
			yyerror(&@2, parse, "No_IPv6 conflicts with No_IP");
			YYERROR;
		}
		parse->current->options.ip_family = FD_DISCOVERY_IPV4;
	}
	| peers NOSCTP ';' {
		if (fd_g_config->cnf_flags.no_tcp ||
		    parse->current->transport == FD_DISCOVERY_DTLS_SCTP) {
			yyerror(&@2, parse, "No_SCTP conflicts with No_TCP");
			YYERROR;
		}
		parse->current->transport = FD_DISCOVERY_TLS_TCP;
	}
	| peers NOTCP ';' {
#if !defined(ENABLE_DTLS_SCTP) || defined(DISABLE_SCTP)
		yyerror(&@2, parse, "No_TCP requires ENABLE_DTLS_SCTP");
		YYERROR;
#endif
		if (fd_g_config->cnf_flags.no_sctp ||
		    parse->current->transport == FD_DISCOVERY_TLS_TCP) {
			yyerror(&@2, parse, "No_TCP conflicts with No_SCTP");
			YYERROR;
		}
		parse->current->transport = FD_DISCOVERY_DTLS_SCTP;
	}
	| peers PREFERTCP ';' {
		/* DNS discovery already prefers TLS/TCP. */
	}
	| peers TCTIMER '=' INTEGER ';' {
		if ($4 > INT_MAX / 200) {
			yyerror(&@4, parse, "Invalid TcTimer");
			YYERROR;
		}
		parse->current->options.tc_timer = $4;
	}
	| peers TWTIMER '=' INTEGER ';' {
		if ($4 > INT_MAX / 2) {
			yyerror(&@4, parse, "Invalid TwTimer");
			YYERROR;
		}
		parse->current->options.tw_timer = $4;
	}
	| peers TLS_PRIO '=' QSTRING ';' {
		if (parse->current->options.tls_priority) {
			yyerror(&@2, parse, "Duplicate TLS_Prio");
			free($4);
			YYERROR;
		}
		parse->current->options.tls_priority = $4;
	}
	| peers REALM '=' QSTRING ';' {
		if (parse->current->options.peer_realm ||
		    !fd_os_is_valid_DiameterIdentity((os0_t)$4, strlen($4))) {
			yyerror(&@2, parse, "Invalid or duplicate peer Realm");
			free($4);
			YYERROR;
		}
		parse->current->options.peer_realm = $4;
	}
	;
%%
