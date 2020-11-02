#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

void background_render(struct tinywl_server *server, struct wlr_output *output, struct wlr_renderer *renderer);
void background_init(struct wlr_renderer *renderer);
void background_deinit();

#endif // BACKGROUND_H
