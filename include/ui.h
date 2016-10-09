#ifndef _UI_H
#define _UI_H

#include <stdbool.h>
#include "worker.h"
#include "termbox.h"
#include "state.h"
#include "render.h"

void init_ui();
void teardown_ui();
void request_rerender(enum render_panels panel);
void rerender();
void rerender_item(size_t index);
void request_fetch(struct aerc_message *message);
bool ui_tick();
int tb_printf(int x, int y, struct tb_cell *basis, const char *fmt, ...);
void add_loading(struct geometry geo);

#endif
