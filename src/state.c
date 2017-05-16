#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "email/headers.h"
#include "config.h"
#include "state.h"
#include "ui.h"
#include "util/time.h"
#include "util/stringop.h"
#include "util/list.h"
#include "worker.h"

void set_status(struct account_state *account, enum account_status state,
		const char *fmt, ...) {
	free(account->status.text);
	if (fmt == NULL) {
		fmt = "Unknown error occured";
	}
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *buf = malloc(len + 1);
	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	account->status.text = buf;
	account->status.status = state;

	get_nanoseconds(&account->status.since);
	request_rerender(PANEL_STATUS_BAR);
}

static int get_mbox_compare(const void *_mbox, const void *_name) {
	const struct aerc_mailbox *mbox = _mbox;
	const char *name = _name;
	return strcmp(mbox->name, name);
}

struct aerc_mailbox *get_aerc_mailbox(struct account_state *account,
		const char *name) {
	if (!account->mailboxes || !name) {
		return NULL;
	}
	int i = list_seq_find(account->mailboxes, get_mbox_compare, name);
	if (i == -1) {
		return NULL;
	}
	return account->mailboxes->items[i];
}

void free_aerc_mailbox(struct aerc_mailbox *mbox) {
	if (!mbox) return;
	free(mbox->name);
	free_flat_list(mbox->flags);
	for (size_t i = 0; i < mbox->messages->length; ++i) {
		struct aerc_message *msg = mbox->messages->items[i];
		free_aerc_message(msg);
	}
	free(mbox);
}

void free_aerc_message_part(struct aerc_message_part *part) {
	if (!part) return;
	free(part->type);
	free(part->subtype);
	free(part->body_id);
	free(part->body_description);
	free(part->body_encoding);
	free(part);
}

void free_aerc_message(struct aerc_message *msg) {
	if (!msg) return;
	free_flat_list(msg->flags);
	if (msg->headers) {
		for (size_t i = 0; i < msg->headers->length; ++i) {
			struct email_header *header = msg->headers->items[i];
			free(header->key);
			free(header->value);
		}
	}
	free_flat_list(msg->headers);
	if (msg->parts) {
		for (size_t i = 0; i < msg->parts->length; ++i) {
			struct aerc_message_part *part = msg->parts->items[i];
			free_aerc_message_part(part);
		}
		list_free(msg->parts);
	}
	free(msg);
}

const char *get_message_header(struct aerc_message *msg, char *key) {
	if (!msg || !msg->headers) {
		return NULL;
	}
	for (size_t i = 0; i < msg->headers->length; ++i) {
		struct email_header *header = msg->headers->items[i];
		if (!header->key) {
			break;
		}
		if (strcasecmp(header->key, key) == 0) {
			return header->value;
		}
	}
	return NULL;
}

bool get_mailbox_flag(struct aerc_mailbox *mbox, char *flag) {
	if (!mbox->flags) return false;
	for (size_t i = 0; i < mbox->flags->length; ++i) {
		const char *_flag = mbox->flags->items[i];
		if (strcmp(flag, _flag) == 0) {
			return true;
		}
	}
	return false;
}

bool get_message_flag(struct aerc_message *msg, char *flag) {
	if (!msg->flags) return false;
	for (size_t i = 0; i < msg->flags->length; ++i) {
		const char *_flag = msg->flags->items[i];
		if (strcmp(flag, _flag) == 0) {
			return true;
		}
	}
	return false;
}

struct account_config *config_for_account(const char *name) {
	for (size_t i = 0; i < config->accounts->length; ++i) {
		struct account_config *c = config->accounts->items[i];
		if (strcmp(c->name, name) == 0) {
			return c;
		}
	}
	return NULL;
}
