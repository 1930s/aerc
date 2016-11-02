#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "util/stringop.h"
#include "commands.h"
#include "config.h"
#include "state.h"
#include "log.h"
#include "ui.h"

static void scroll_selected_into_view() {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	int relative = account->ui.selected_message - account->ui.list_offset;
	int height = state->panels.message_list.height - 1;
	if (relative >= height) {
		account->ui.list_offset += relative - height;
		request_rerender(PANEL_MESSAGE_LIST);
	} else if (relative < 0) {
		account->ui.list_offset += relative;
		request_rerender(PANEL_MESSAGE_LIST);
	}
}

static void handle_quit(int argc, char **argv) {
	// TODO: We may occasionally want to confirm the user's choice here
	state->exit = true;
}

static void handle_reload() {
	load_main_config(NULL);
}

static void handle_message_seek(char *cmd, int mul, int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	int amt = 1;
	bool scroll = false;
	if (argc > 0) {
		if (strcmp(argv[0], "--scroll") == 0) {
			scroll = true;
			argv = &argv[1];
			argc--;
		}
		char *end;
		amt = strtol(argv[0], &end, 10);
		if (end == argv[0]) {
			set_status(account, ACCOUNT_ERROR, "Usage: %s [--scroll] [amount|%]", cmd);
			return;
		}
		if (*end) {
			if (end[0] == '%' && !end[1]) {
				amt = state->panels.message_list.height * (amt / 100.0);
			} else {
				set_status(account, ACCOUNT_ERROR, "Usage: %s [--scroll] [amount|%]", cmd);
				return;
			}
		}
	}
	amt *= mul;
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, account->selected);
	if (!mbox) {
		return;
	}
	int new = (int)account->ui.selected_message + amt;
	if (new < 0) amt -= new;
	if (new >= (int)mbox->messages->length) amt -= new - mbox->messages->length + 1;
	if (scroll) {
		account->ui.list_offset += amt;
	}
	account->ui.selected_message += amt;
	scroll_selected_into_view();
	request_rerender(PANEL_MESSAGE_LIST);
}

static void handle_next_message(int argc, char **argv) {
	handle_message_seek("next-message", 1, argc, argv);
}

static void handle_previous_message(int argc, char **argv) {
	handle_message_seek("previous-message", -1, argc, argv);
}

static void handle_select_message(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	if (argc != 1) {
		set_status(account, ACCOUNT_ERROR, "Usage: select-message [n]");
		return;
	}
	char *end;
	int requested = strtol(argv[0], &end, 10);
	if (end == argv[0] || *end) {
		set_status(account, ACCOUNT_ERROR, "Usage: select-message [n]");
		return;
	}
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, account->selected);
	if (!mbox) {
		return;
	}
	if (requested < 0) {
		requested = mbox->messages->length + requested;
	}
	if (requested > (int)mbox->messages->length) {
		set_status(account, ACCOUNT_ERROR, "Requested message is out of range.");
		return;
	}
	account->ui.selected_message = requested;
	scroll_selected_into_view();
	request_rerender(PANEL_MESSAGE_LIST);
}

static void handle_next_account(int argc, char **argv) {
	state->selected_account++;
	state->selected_account %= state->accounts->length;
	request_rerender(PANEL_ALL);
}

static void handle_previous_account(int argc, char **argv) {
	state->selected_account--;
	state->selected_account %= state->accounts->length;
	request_rerender(PANEL_ALL);
}

static void handle_next_folder(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	int i = -1;
	worker_log(L_DEBUG, "Current: %s", account->selected);
	for (i = 0; i < (int)account->mailboxes->length; ++i) {
		struct aerc_mailbox *mbox = account->mailboxes->items[i];
		if (!mbox) {
			return;
		}
		if (!strcmp(mbox->name, account->selected)) {
			break;
		}
	}
	if (i == -1 || i == (int)account->mailboxes->length) {
		return;
	}
	i++;
	i %= account->mailboxes->length;
	struct aerc_mailbox *next = account->mailboxes->items[i];
	worker_post_action(account->worker.pipe, WORKER_SELECT_MAILBOX,
			NULL, strdup(next->name));
	request_rerender(PANEL_SIDEBAR | PANEL_MESSAGE_LIST);
}

static void handle_previous_folder(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	int i = -1;
	for (i = 0; i < (int)account->mailboxes->length; ++i) {
		struct aerc_mailbox *mbox = account->mailboxes->items[i];
		if (!mbox) {
			return;
		}
		if (!strcmp(mbox->name, account->selected)) {
			break;
		}
	}
	if (i == -1 || i == (int)account->mailboxes->length) {
		return;
	}
	i--;
	if (i == -1) {
		i = account->mailboxes->length - 1;
	}
	struct aerc_mailbox *next = account->mailboxes->items[i];
	worker_post_action(account->worker.pipe, WORKER_SELECT_MAILBOX,
			NULL, strdup(next->name));
	request_rerender(PANEL_SIDEBAR | PANEL_MESSAGE_LIST);
}

static void handle_cd(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	char *joined = join_args(argv, argc);
	worker_post_action(account->worker.pipe, WORKER_SELECT_MAILBOX,
			NULL, joined);
	request_rerender(PANEL_SIDEBAR | PANEL_MESSAGE_LIST);
}

static void handle_delete_mailbox(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	char *joined = join_args(argv, argc);
	// TODO: Are you sure?
	worker_post_action(account->worker.pipe, WORKER_DELETE_MAILBOX,
			NULL, strdup(joined));
	free(joined);
	request_rerender(PANEL_SIDEBAR | PANEL_MESSAGE_LIST);
}

static void handle_set(int argc, char **argv) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	if (argc < 2) {
		set_status(account, ACCOUNT_ERROR, "Usage: set [section].[key] [value]");
		return;
	}
	char *seckey = argv[0];
	char *dot = strchr(seckey, '.');
	if (!dot) {
		set_status(account, ACCOUNT_ERROR, "Usage: set [section].[key] [value]");
		return;
	}
	*dot = '\0';
	char *section = seckey;
	char *key = dot + 1;
	char *value = join_args(argv + 1, argc - 1);
	handle_config_option(config, section, key, value);
	request_rerender(PANEL_ALL);
	set_status(account, ACCOUNT_OKAY, "Connected.");
	free(value);
}

struct cmd_handler {
	char *command;
	void (*handler)(int argc, char **argv);
};

// Keep alphabetized, please
struct cmd_handler cmd_handlers[] = {
	{ "cd", handle_cd },
	{ "delete-mailbox", handle_delete_mailbox },
	{ "exit", handle_quit },
	{ "next-account", handle_next_account },
	{ "next-folder", handle_next_folder },
	{ "next-message", handle_next_message },
	{ "previous-account", handle_previous_account },
	{ "previous-folder", handle_previous_folder },
	{ "previous-message", handle_previous_message },
	{ "q", handle_quit },
	{ "quit", handle_quit },
	{ "reload", handle_reload },
	{ "select-message", handle_select_message },
	{ "set", handle_set },
};

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

static struct cmd_handler *find_handler(char *line) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = NULL;
	worker_log(L_DEBUG, "find_handler(%s)", line);
	res = bsearch(&d, cmd_handlers,
		sizeof(cmd_handlers) / sizeof(struct cmd_handler),
		sizeof(struct cmd_handler), handler_compare);
	return res;
}

void handle_command(const char *_exec) {
	char *exec = strdup(_exec);
	char *head = exec;
	char *cmdlist;
	char *cmd;

	head = exec;
	do {
		// Split command list
		cmdlist = argsep(&head, ";");
		cmdlist += strspn(cmdlist, whitespace);
		do {
			// Split commands
			cmd = argsep(&cmdlist, ",");
			cmd += strspn(cmd, whitespace);
			if (strcmp(cmd, "") == 0) {
				worker_log(L_DEBUG, "Ignoring empty command.");
				continue;
			}
			worker_log(L_DEBUG, "Handling command '%s'", cmd);
			//TODO better handling of argv
			int argc;
			char **argv = split_args(cmd, &argc);
			if (strcmp(argv[0], "exec") != 0) {
				int i;
				for (i = 1; i < argc; ++i) {
					if (*argv[i] == '\"' || *argv[i] == '\'') {
						strip_quotes(argv[i]);
					}
				}
			}
			struct cmd_handler *handler = find_handler(argv[0]);
			if (!handler) {
				worker_log(L_DEBUG, "Unknown command %s", argv[0]);
				free_argv(argc, argv);
				goto cleanup;
			}
			handler->handler(argc-1, argv+1);
			free_argv(argc, argv);
		} while(cmdlist);
	} while(head);
	cleanup:
	free(exec);
}
