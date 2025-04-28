/* cc_box.c - generic wrapper implementation */
#include "cc_box_api.h"
#include <stdlib.h>

cc_box_t *
cc_box_create(const cc_algo_t *algo, void *params)
{
	if (!algo || !algo->init || !algo->on_event || !algo->get_rate)
		return NULL;

	cc_box_t *box = malloc(sizeof(*box));
	if (!box)
		return NULL;

	box->algo  = algo;
	box->state = algo->init(params);
	if (!box->state) {
		free(box);
		return NULL;
	}

	return box;
}

void
cc_box_destroy(cc_box_t *box)
{
	if (!box)
		return;

	box->algo->cleanup(box->state);
	free(box);
}

void
cc_box_event(cc_box_t *box, enum cc_event_t evt)
{
	if (!box)
		return;

	box->algo->on_event(box->state, evt);
}

uint32_t
cc_box_get_rate(const cc_box_t *box)
{
	return box ? box->algo->get_rate(box->state) : 0U;
}