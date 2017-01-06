/*
 * imap/worker/fetch.c - Handles the WORKER_FETCH_* actions
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>

#include "imap/imap.h"
#include "worker.h"

void handle_worker_fetch_messages(struct worker_pipe *pipe,
		struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	struct message_range *range = message->data;

	imap_arg_t *args = calloc(1, sizeof(imap_arg_t));

	const char *what = "UID FLAGS INTERNALDATE BODYSTRUCTURE BODY.PEEK["
			"HEADER.FIELDS (DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES "
			"CONTENT-TYPE IN-REPLY-TO REPLY-TO)]";

	imap_fetch(imap, NULL, NULL, range->min, range->max, what);

	imap_arg_free(args);
	free(range);
}
