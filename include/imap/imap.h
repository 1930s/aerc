#ifndef _IMAP_H
#define _IMAP_H

// Implements RFC 3501 (Internet Mail Access Protocol)

#include <poll.h>
#include <stdbool.h>

#include "absocket.h"
#include "urlparse.h"
#include "util/hashtable.h"
#include "util/list.h"
#include "util/time.h"

// TODO: Refactor these into the internal header:
// - recv_mode
// - struct imap_connection

struct imap_capabilities {
	bool imap4rev1;
	bool starttls;
	bool logindisabled;
	bool auth_plain;
	bool auth_login;
	bool idle;
	bool sasl_ir;
};

enum imap_status {
	STATUS_OK, STATUS_NO, STATUS_BAD, STATUS_PREAUTH, STATUS_BYE,
	STATUS_PRE_ERROR // Returned when our code anticipates an error
};

enum recv_mode {
	RECV_WAIT,
	RECV_LINE,
	RECV_IDLE,
};

struct imap_connection;

typedef void (*imap_callback_t)(struct imap_connection *imap,
		void *data, enum imap_status status, const char *args);

struct mailbox_flag {
	char *name;
	bool permanent;
};

struct message_parameter {
	char *key, *value;
};

struct message_part {
	char *type;
	char *subtype;
	list_t *parameters;
	char *body_id;
	char *body_description;
	char *body_encoding;
	long size;
	uint8_t *content;
};

struct mailbox_message {
	bool fetching, populated;
	int index;
	long uid;
	list_t *flags, *headers;
	struct tm *internal_date;
	char *multipart_type;
	list_t *parts;
};

struct mailbox {
	list_t *flags;
	list_t *messages;
	char *name;
	long exists, recent, unseen;
	long nextuid; // Predicted, not definite
	bool read_write;
	bool selected;
};

struct imap_connection {
	struct {
		void (*mailbox_updated)(struct imap_connection *, struct mailbox *mbox);
		void (*mailbox_deleted)(struct imap_connection *, const char *name);
		void (*message_updated)(struct imap_connection *, struct mailbox_message *);
		void (*message_deleted)(struct imap_connection *, struct mailbox_message *);
	} events;

	void *data;
	bool logged_in;
	struct timespec idle_start;
	struct timespec last_network;
	absocket_t *socket;
	enum recv_mode mode;
	char *line;
	int line_index, line_size;
	struct pollfd poll[1];
	int next_tag;
	hashtable_t *pending;
	struct imap_capabilities *cap;
	struct imap_state *state;
	struct uri *uri;
	list_t *mailboxes;
	char *selected;
	list_t *select_queue;
};

enum imap_type {
	IMAP_ATOM,	  /* imap->str is valid */
	IMAP_NUMBER,	/* imap->num is valid */
	IMAP_STRING,	/* imap->str is valid */
	IMAP_LIST,	  /* imap->list is valid */
	IMAP_RESPONSE,  /* imap->str is valid */
	IMAP_NIL		/* Not actually used, will be IMAP_ATOM and imap->str will
						equal "NIL" */
};

struct imap_arg {
	enum imap_type type;
	struct imap_arg *next;
	char *str;
	long num;
	struct imap_arg *list;
	char *original;
};
typedef struct imap_arg imap_arg_t;

void imap_arg_free(imap_arg_t *args);

bool imap_connect(struct imap_connection *imap, const struct uri *uri,
		bool use_ssl, imap_callback_t callback, void *data);
int imap_receive(struct imap_connection *imap);
void imap_send(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *fmt, ...);
void imap_close(struct imap_connection *imap);

void imap_list(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *refname, const char *boxname);
void imap_capability(struct imap_connection *imap, imap_callback_t callback,
		void *data);
void imap_select(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox);
void imap_fetch(struct imap_connection *imap, imap_callback_t callback,
		void *data, size_t min, size_t max, const char *what);
void imap_delete(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox);

#endif
