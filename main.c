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
            p.rate_max = atoi(argv[2]);
            param = &p;
        } else {
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

    if (!strcmp(argv[1], "reno")) {
        for (i = 0; i < 5; i++) {
            cc_box_event(box, CC_EVT_ACK);
            printf("[%s] after ACK %d: rate/cwnd = %u\n",
                argv[1], i + 1, cc_box_get_rate(box));
        }

        cc_box_event(box, CC_EVT_ECN);
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
    } else if (!strcmp(argv[1], "dcqcn")) {
        for (i = 0; i < 5; i++) {
            cc_box_event(box, CC_EVT_ALPHA_TIMER);
            cc_box_event(box, CC_EVT_RATE_INCREASE_TIMER);
            printf("[%s] after CC_EVT_RATE_INCREASE_TIMER %d: rate/cwnd = %u\n",
                argv[1], i + 1, cc_box_get_rate(box));
        }

        for (i = 0; i < 5; i++) {
            cc_box_event(box, CC_EVT_ECN);
            printf("[%s] after CC_EVT_ECN: rate/cwnd = %u\n",
                argv[1], cc_box_get_rate(box));
        }

        for (i = 0; i < 20; i++) {
            cc_box_event(box, CC_EVT_ALPHA_TIMER);
            cc_box_event(box, CC_EVT_BYTE_COUNTER);
            printf("[%s] after CC_EVT_BYTE_COUNTER %d: rate/cwnd = %u\n",
                argv[1], i + 1, cc_box_get_rate(box));
        }

        for (i = 0; i < 10; i++) {
            cc_box_event(box, CC_EVT_ECN);
            printf("[%s] after CC_EVT_ECN: rate/cwnd = %u\n",
                argv[1], cc_box_get_rate(box));
        }

        for (i = 0; i < 20; i++) {
            cc_box_event(box, CC_EVT_ALPHA_TIMER);
            cc_box_event(box, CC_EVT_RATE_INCREASE_TIMER);
            printf("[%s] after CC_EVT_RATE_INCREASE_TIMER %d: rate/cwnd = %u\n",
                argv[1], i + 1, cc_box_get_rate(box));
        }

        for (i = 0; i < 20; i++) {
            cc_box_event(box, CC_EVT_ALPHA_TIMER);
            cc_box_event(box, CC_EVT_BYTE_COUNTER);
            printf("[%s] after CC_EVT_BYTE_COUNTER %d: rate/cwnd = %u\n",
                argv[1], i + 1, cc_box_get_rate(box));
        }
    }

    cc_box_destroy(box);
	return EXIT_SUCCESS;
}