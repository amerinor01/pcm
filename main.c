/* main.c - test harness for Reno and DCQCN */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc_box_api.h"
#include "dcqcn_params.h"

extern const cc_algo_t reno_algo;
extern const cc_algo_t dcqcn_algo;

static void
usage(const char *prog)
{
	printf("Usage: %s <reno|dcqcn> [param]\n", prog);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	const cc_algo_t *algo;
	void            *param = NULL;
	int              i;

	if (argc < 2)
		usage(argv[0]);

	if (!strcmp(argv[1], "reno")) {
		algo = &reno_algo;
	} else if (!strcmp(argv[1], "dcqcn")) {
		algo = &dcqcn_algo;
        if (argc > 2) {
            dcqcn_params_t p;
            p.rate_max = (argc>4 ? atoi(argv[2]) : 50000000U);
            param = &p;
        } else {
            /* no params => NULL to init => defaults in dcqcn_init */
            param = NULL;
        }
	} else {
		usage(argv[0]);
	}

	cc_box_t *box = cc_box_create(algo, param);
	if (!box) {
		fprintf(stderr, "cc_box_create failed\n");
		return EXIT_FAILURE;
	}

	printf("[%s] init rate/cwnd = %u\n",
	       argv[1], cc_box_get_rate(box));

	for (i = 0; i < 5; i++) {
		cc_box_event(box, CC_EVT_ACK);
		printf("[%s] after ACK %d: rate/cwnd = %u\n",
		       argv[1], i + 1, cc_box_get_rate(box));
	}

	cc_box_event(box, CC_EVT_ECN);  /* e.g. 0.1 * 1e6 */
	printf("[%s] after ECN: rate/cwnd = %u\n",
	       argv[1], cc_box_get_rate(box));

	cc_box_event(box, CC_EVT_NACK);
	printf("[%s] after loss: rate/cwnd = %u\n",
	       argv[1], cc_box_get_rate(box));

	cc_box_event(box, CC_EVT_TIMEOUT);
	printf("[%s] after RTO: rate/cwnd = %u\n",
	       argv[1], cc_box_get_rate(box));

    for (i = 0; i < 20; i++) {
        cc_box_event(box, CC_EVT_ACK);
        printf("[%s] after ACK %d: rate/cwnd = %u\n",
               argv[1], i + 1, cc_box_get_rate(box));
    }

    cc_box_destroy(box);
	return EXIT_SUCCESS;
}