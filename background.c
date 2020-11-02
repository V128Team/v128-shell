#include "tinywl.h"

#include "v128-logo.h"
#include "background.h"
#include "log.h"

static struct wlr_texture *background = NULL;

void background_render(struct tinywl_server *server, struct wlr_output *output, struct wlr_renderer *renderer) {
  int mode_width = output->width;
  int mode_height = output->height;

  float color[4] = {0.3, 0.3, 0.3, 1.0};
  wlr_renderer_clear(renderer, color);

  /* Render the background logo first */
  struct wlr_box box = {
    .x = (mode_width / 2) - (v128_logo.width / 2),
    .y = (mode_height / 2) - (v128_logo.height / 2),
    .width = v128_logo.width, .height = v128_logo.height
  };

  double local_x = box.x;
  double local_y = box.y;
  wlr_output_layout_output_coords(server->output_layout, output, &local_x, &local_y);
  wlr_render_texture(renderer, background, output->transform_matrix, local_x, local_y, 1.0f);
}

void background_init(struct wlr_renderer *renderer) {
  background = wlr_texture_from_pixels(renderer,
                                       WL_SHM_FORMAT_ABGR8888,
                                       v128_logo.width * 4,
                                       v128_logo.width,
                                       v128_logo.height,
                                       v128_logo.pixel_data);
}

void background_deinit() {
  wlr_texture_destroy(background);
}
