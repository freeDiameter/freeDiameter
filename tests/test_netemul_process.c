#include "tests.h"
#include <math.h>

static double random_values[4];
static size_t random_count;
static size_t random_index;

static double test_random(void)
{
	CHECK(1, random_index < random_count);
	return random_values[random_index++];
}

/* Control the random inputs while exercising the extension's actual queue logic. */
#define drand48 test_random
#include "../extensions/test_netemul/tne_process.c"
#undef drand48

struct tne_conf tne_conf;

static void check_duplicates(double probability, double sample, int expected)
{
	struct dict_object *model = NULL;
	struct dict_object *user_name = NULL;
	struct msg *original = NULL;
	struct msg_hdr *original_hdr;
	struct avp *avp;
	union avp_value value;
	struct process_item item = { 0 };
	struct fd_list *li;
	int count = 0;

	CHECK(0, fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME,
				"Accounting-Request", &model, ENOENT));
	CHECK(0, fd_msg_new(model, MSGFL_ALLOC_ETEID, &original));
	CHECK(0, fd_msg_source_set(original, "peer.localdomain", 16));
	CHECK(0, fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,
				"User-Name", &user_name, ENOENT));
	CHECK(0, fd_msg_avp_new(user_name, 0, &avp));
	value.os.data = (unsigned char *)"test-user";
	value.os.len = 9;
	CHECK(0, fd_msg_avp_setvalue(avp, &value));
	CHECK(0, fd_msg_avp_add(original, MSG_BRW_LAST_CHILD, avp));
	CHECK(0, fd_msg_hdr(original, &original_hdr));
	fd_list_init(&item.chain, original);
	fd_list_insert_before(&input, &item.chain);

	tne_conf.dupl_proba = probability;
	random_values[0] = sample;
	random_count = probability != 0.0 ? 1 : 0;
	random_index = 0;
	CHECK(0, do_duplicates());
	CHECK(random_count, random_index);

	while (!FD_IS_LIST_EMPTY(&forlat)) {
		struct process_item *pi;
		struct msg_hdr *hdr;
		struct avp_hdr *avp_hdr;
		DiamId_t source;
		size_t source_len;

		li = forlat.next;
		pi = (struct process_item *)li;
		CHECK(0, fd_msg_hdr(li->o, &hdr));
		CHECK(original_hdr->msg_eteid, hdr->msg_eteid);
		CHECK(original_hdr->msg_hbhid, hdr->msg_hbhid);
		CHECK(count == 0 ? 0 : CMD_FLAG_RETRANSMIT,
		      hdr->msg_flags & CMD_FLAG_RETRANSMIT);
		CHECK(0, fd_msg_source_get(li->o, &source, &source_len));
		CHECK(16, source_len);
		CHECK(0, memcmp(source, "peer.localdomain", source_len));
		CHECK(0, fd_msg_parse_dict(li->o, fd_g_config->cnf_dict, NULL));
		CHECK(0, fd_msg_search_avp(li->o, user_name, &avp));
		CHECK(1, avp != NULL);
		CHECK(0, fd_msg_avp_hdr(avp, &avp_hdr));
		CHECK(9, avp_hdr->avp_value->os.len);
		CHECK(0, memcmp(avp_hdr->avp_value->os.data, "test-user", 9));
		fd_list_unlink(li);
		if (pi != &item) {
			CHECK(0, fd_msg_free(li->o));
			free(pi);
		}
		count++;
	}
	CHECK(expected, count);
	CHECK(0, fd_msg_free(original));
}

int main(int argc, char *argv[])
{
	struct process_item first = { 0 }, second = { 0 }, middle = { 0 };

	INIT_FD();

	check_duplicates(0.0, 0.0, 1);
	check_duplicates(1.0, 0.5, 2);
	check_duplicates(1.0, 0.005, 4);
	check_duplicates(1.0, 0.0, 2);
	check_duplicates(0.25, 0.5, 1);

	first.ts.tv_sec = second.ts.tv_sec = 100;
	fd_list_init(&first.chain, &first);
	fd_list_init(&second.chain, &second);
	fd_list_insert_before(&forlat, &first.chain);
	fd_list_insert_before(&forlat, &second.chain);
	tne_conf.lat_avg = 1000;
	tne_conf.lat_dev = 100;
	random_values[0] = 0.75;
	random_values[1] = 0.5;
	random_values[2] = 0.25;
	random_values[3] = 0.5;
	random_count = 4;
	random_index = 0;
	CHECK(0, do_latency());
	CHECK(4, random_index);
	CHECK(1, first.ts.tv_sec > 101);
	CHECK(100, second.ts.tv_sec);
	CHECK(1, waitlist.next == &second.chain);
	CHECK(1, second.chain.next == &first.chain);

	middle.ts.tv_sec = 101;
	fd_list_init(&middle.chain, &middle);
	fd_list_insert_before(&forlat, &middle.chain);
	tne_conf.lat_dev = 0;
	random_count = random_index = 0;
	CHECK(0, do_latency());
	CHECK(0, random_index);
	CHECK(102, middle.ts.tv_sec);
	CHECK(1, second.chain.next == &middle.chain);
	CHECK(1, middle.chain.next == &first.chain);
	CHECK(1, first.chain.next == &waitlist);
	fd_list_unlink(&first.chain);
	fd_list_unlink(&second.chain);
	fd_list_unlink(&middle.chain);

	PASSTEST();
}
