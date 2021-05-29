#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "tracy/TracyC.h"

#include "tinywl.h"

#include "log.h"
#include "subprogram.h"
#include "background.h"

static void focus_view(struct tinywl_view *view, struct wlr_surface *surface) {
  /* Note: this function only deals with keyboard focus. */
  if (view == NULL) {
    printf("focus_view: No view, so returning.\n");
    return;
  }

  struct tinywl_server *server = view->server;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

  if (prev_surface == surface) {
    /* Don't re-focus an already focused surface. */
    printf("focus_view: View already focused, so returning.\n");
    return;
  }

  if (prev_surface) {
    /*
     * Deactivate the previously focused surface. This lets the client know
     * it no longer has focus and the client will repaint accordingly, e.g.
     * stop displaying a caret.
     */
    printf("focus_view: Deactivating previous surface.\n");
    struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
        seat->keyboard_state.focused_surface);
    wlr_xdg_toplevel_set_activated(previous, false);
  }

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

  /* Move the view to the front */
  wl_list_remove(&view->link);
  wl_list_insert(&server->views, &view->link);

  /* Activate the new surface */
  printf("focus_view: Setting surface activated.\n");
  wlr_xdg_toplevel_set_activated(view->xdg_surface, true);

  /*
   * Tell the seat to have the keyboard enter this surface. wlroots will keep
   * track of this and automatically send key events to the appropriate
   * clients without additional work on your part.
   */
  printf("focus_view: wlr_seat_keyboard_notify_enter.\n");
  wlr_seat_keyboard_notify_enter(seat, view->xdg_surface->surface,
                                 keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
  /* This event is raised when a modifier key, such as shift or alt, is
   * pressed. We simply communicate this to the client. */
  struct tinywl_keyboard *keyboard =
      wl_container_of(listener, keyboard, modifiers);

  /*
   * A seat can only have one keyboard, but this is a limitation of the
   * Wayland protocol - not wlroots. We assign all connected keyboards to the
   * same seat. You can swap out the underlying wlr_keyboard like this and
   * wlr_seat handles this transparently.
   */
  wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);

  /* Send modifiers to the client. */
  wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
                                     &keyboard->device->keyboard->modifiers);
}

static void cycle_view(struct tinywl_server *server) {
  /* Cycle to the next view */
  if (wl_list_length(&server->views) < 2) {
    return;
  }

  struct tinywl_view *current_view = wl_container_of(
      server->views.next, current_view, link);
  struct tinywl_view *next_view = wl_container_of(
      current_view->link.next, next_view, link);
  focus_view(next_view, next_view->xdg_surface->surface);

  /* Move the previous view to the end of the list */
  wl_list_remove(&current_view->link);
  wl_list_insert(server->views.prev, &current_view->link);
}

static bool handle_keybinding(struct tinywl_server *server, xkb_keysym_t sym) {
  /*
   * Here we handle compositor keybindings. This is when the compositor is
   * processing keys, rather than passing them on to the client for its own
   * processing.
   *
   * This function assumes Alt is held down.
   */
  switch (sym) {
	case XKB_KEY_q:
      wl_display_terminate(server->wl_display);
      break;

	case XKB_KEY_bracketright:
      cycle_view(server);
      break;

    case XKB_KEY_t:
      subprogram_start("cool-retro-term");
      break;

    case XKB_KEY_v:
      subprogram_start("SDL_VIDEODRIVER=wayland x128");
      break;

	default:
      return false;
  }

  return true;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
  /* This event is raised when a key is pressed or released. */
  struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, key);
  struct tinywl_server *server = keyboard->server;
  struct wlr_event_keyboard_key *event = data;
  struct wlr_seat *seat = server->seat;

  /* Translate libinput keycode -> xkbcommon */
  uint32_t keycode = event->keycode + 8;

  /* Get a list of keysyms based on the keymap for this keyboard */
  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(
      keyboard->device->keyboard->xkb_state, keycode, &syms);

  bool handled = false;
  uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
  if ((modifiers & WLR_MODIFIER_ALT) && event->state == WLR_KEY_PRESSED) {
    /* If alt is held down and this button was _pressed_, we attempt to
     * process it as a compositor keybinding. */
    for (int i = 0; i < nsyms; i++) {
      handled = handle_keybinding(server, syms[i]);
    }
  }

  if (!handled) {
    /* Otherwise, we pass it along to the client. */
    wlr_seat_set_keyboard(seat, keyboard->device);
    wlr_seat_keyboard_notify_key(seat, event->time_msec,
                                 event->keycode, event->state);
  }
}

static void server_new_keyboard(struct tinywl_server *server, struct wlr_input_device *device) {
  struct tinywl_keyboard *keyboard = calloc(1, sizeof(struct tinywl_keyboard));
  keyboard->server = server;
  keyboard->device = device;

  /* We need to prepare an XKB keymap and assign it to the keyboard. This
   * assumes the defaults (e.g. layout = "us"). */
  struct xkb_rule_names rules = { 0 };
  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
                                                     XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(device->keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

  /* Here we set up listeners for keyboard events. */
  keyboard->modifiers.notify = keyboard_handle_modifiers;
  wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
  keyboard->key.notify = keyboard_handle_key;
  wl_signal_add(&device->keyboard->events.key, &keyboard->key);

  wlr_seat_set_keyboard(server->seat, device);

  /* And add the keyboard to our list of keyboards */
  wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_input(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new input device becomes
   * available. */
  struct tinywl_server *server =
      wl_container_of(listener, server, new_input);
  struct wlr_input_device *device = data;

  switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
      server_new_keyboard(server, device);
      break;
	default:
      break;
  }

  /* We need to let the wlr_seat know what our capabilities are, which is
   * communiciated to the client. In TinyWL we always have a cursor, even if
   * there are no pointer devices, so we always include that capability. */
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

  if (!wl_list_empty(&server->keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }

  wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
  /* This event is raised by the seat when a client wants to set the selection,
   * usually when the user copies something. wlroots allows compositors to
   * ignore such requests if they so choose, but in tinywl we always honor
   */
  struct tinywl_server *server = wl_container_of(
      listener, server, request_set_selection);
  struct wlr_seat_request_set_selection_event *event = data;

  wlr_seat_set_selection(server->seat, event->source, event->serial);
}

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
struct render_data {
  struct wlr_output *output;
  struct wlr_renderer *renderer;
  struct tinywl_view *view;
  struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
  TracyCZoneNS(render_surface_ctx, "render_surface", 10, true);

  char frame_name[256] = {};
  snprintf(frame_name, sizeof(*frame_name)-1, "surface 0x%" PRIXPTR, (uintptr_t)(surface));
  TracyCFrameMarkStart(frame_name);

  /* This function is called for every surface that needs to be rendered. */
  struct render_data *rdata = data;
  struct tinywl_view *view = rdata->view;
  struct wlr_output *output = rdata->output;

  /* We first obtain a wlr_texture, which is a GPU resource. wlroots
   * automatically handles negotiating these with the client. The underlying
   * resource could be an opaque handle passed from the client, or the client
   * could have sent a pixel buffer which we copied to the GPU, or a few other
   * means. You don't have to worry about this, wlroots takes care of it. */
  TracyCMessageL("wlr_surface_get_texture");
  struct wlr_texture *texture = wlr_surface_get_texture(surface);
  if (texture == NULL) {
    TracyCMessageLS("Surface texture was NULL", 10);
    TracyCFrameMarkEnd(frame_name);
    TracyCZoneEnd(render_surface_ctx);
    return;
  }

  /* The view has a position in layout coordinates. If you have two displays,
   * one next to the other, both 1080p, a view on the rightmost display might
   * have layout coordinates of 2000,100. We need to translate that to
   * output-local coordinates, or (2000 - 1920). */
  double ox = 0, oy = 0;
  wlr_output_layout_output_coords(
      view->server->output_layout, output, &ox, &oy);
  ox += view->x + sx, oy += view->y + sy;

  /* We also have to apply the scale factor for HiDPI outputs. This is only
   * part of the puzzle, TinyWL does not fully support HiDPI. */
  struct wlr_box box = {
    .x = ox * output->scale,
    .y = oy * output->scale,
    .width = surface->current.width * output->scale,
    .height = surface->current.height * output->scale,
  };

  /*
   * Those familiar with OpenGL are also familiar with the role of matricies
   * in graphics programming. We need to prepare a matrix to render the view
   * with. wlr_matrix_project_box is a helper which takes a box with a desired
   * x, y coordinates, width and height, and an output geometry, then
   * prepares an orthographic projection and multiplies the necessary
   * transforms to produce a model-view-projection matrix.
   *
   * Naturally you can do this any way you like, for example to make a 3D
   * compositor.
   */
  float matrix[9];
  enum wl_output_transform transform =
      wlr_output_transform_invert(surface->current.transform);
  wlr_matrix_project_box(matrix, &box, transform, 0,
                         output->transform_matrix);

  /* This takes our matrix, the texture, and an alpha, and performs the actual
   * rendering on the GPU. */
  TracyCMessageL("wlr_render_texture_with_matrix");
  wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

  /* This lets the client know that we've displayed that frame and it can
   * prepare another one now if it likes. */
  TracyCMessageL("wlr_surface_send_frame_done");
  wlr_surface_send_frame_done(surface, rdata->when);

  TracyCFrameMarkEnd(frame_name);
  TracyCZoneEnd(render_surface_ctx);
}

static void output_frame(struct wl_listener *listener, void *data) {
  TracyCZoneNS(output_frame_ctx, "output_frame", 10, true);

  /* This function is called every time an output is ready to display a frame,
   * generally at the output's refresh rate (e.g. 60Hz). */
  struct tinywl_output *output =
      wl_container_of(listener, output, frame);
  struct wlr_renderer *renderer = output->server->renderer;

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  /* wlr_output_attach_render makes the OpenGL context current. */
  TracyCMessageL("wlr_output_attach_render");
  if (!wlr_output_attach_render(output->wlr_output, NULL)) {
    TracyCMessageLS("wlr_output_attach_render failed", 10);
    TracyCFrameMark;
    TracyCZoneEnd(output_frame_ctx);
    return;
  }

  /* The "effective" resolution can change if you rotate your outputs. */
  int width, height;
  wlr_output_effective_resolution(output->wlr_output, &width, &height);

  /* Begin the renderer (calls glViewport and some other GL sanity checks) */
  TracyCMessageL("wlr_renderer_begin");
  wlr_renderer_begin(renderer, width, height);

  TracyCMessageL("background_render");
  background_render(output->server, output->wlr_output, renderer);

  /* Each subsequent window we render is rendered on top of the last. Because
   * our view list is ordered front-to-back, we iterate over it backwards. */
  struct tinywl_view *view;
  wl_list_for_each_reverse(view, &output->server->views, link) {
    if (!view->mapped) {
      /* An unmapped view should not be rendered. */
      continue;
    }
    struct render_data rdata = {
      .output = output->wlr_output,
      .view = view,
      .renderer = renderer,
      .when = &now,
    };
    TracyCMessageL("wlr_xdg_surface_for_each_surface");
    /* This calls our render_surface function for each surface among the
     * xdg_surface's toplevel and popups. */
    wlr_xdg_surface_for_each_surface(view->xdg_surface,
                                     render_surface, &rdata);
  }

  /* Conclude rendering and swap the buffers, showing the final frame
   * on-screen. */
  TracyCMessageL("wlr_renderer_end");
  wlr_renderer_end(renderer);
  TracyCMessageL("wlr_output_commit");
  wlr_output_commit(output->wlr_output);

  TracyCZoneEnd(output_frame_ctx);
  TracyCFrameMark;
}

static void server_new_output(struct wl_listener *listener, void *data) {
  LOG("New display output found.")

  /* This event is rasied by the backend when a new output (aka a display or
   * monitor) becomes available. */
  struct tinywl_server *server = wl_container_of(listener, server, new_output);
  struct wlr_output *wlr_output = data;

  /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
   * before we can use the output. The mode is a tuple of (width, height,
   * refresh rate), and each monitor supports only a specific set of modes. We
   * just pick the monitor's preferred mode, a more sophisticated compositor
   * would let the user configure it. */
  if (!wl_list_empty(&wlr_output->modes)) {
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    wlr_output_set_mode(wlr_output, mode);
    wlr_output_enable(wlr_output, true);
    if (!wlr_output_commit(wlr_output)) {
      return;
    }
  }

  /* Allocates and configures our state for this output */
  struct tinywl_output *output = calloc(1, sizeof(struct tinywl_output));
  output->wlr_output = wlr_output;
  output->server = server;

  /* Sets up a listener for the frame notify event. */
  output->frame.notify = output_frame;
  wl_signal_add(&wlr_output->events.frame, &output->frame);
  wl_list_insert(&server->outputs, &output->link);

  /* Adds this to the output layout. The add_auto function arranges outputs
   * from left-to-right in the order they appear. A more sophisticated
   * compositor would let the user configure the arrangement of outputs in the
   * layout.
   *
   * The output layout utility automatically adds a wl_output global to the
   * display, which Wayland clients can see to find out information about the
   * output (such as DPI, scale factor, manufacturer, etc).
   */
  wlr_output_layout_add_auto(server->output_layout, wlr_output);
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
  struct tinywl_view *view = wl_container_of(listener, view, map);

  /* Called when the surface is mapped, or ready to display on-screen. */
  view->mapped = true;
  focus_view(view, view->xdg_surface->surface);
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct tinywl_view *view = wl_container_of(listener, view, unmap);
  view->mapped = false;
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data) {
  /* Called when the surface is destroyed and should never be shown again. */
  struct tinywl_view *view = wl_container_of(listener, view, destroy);
  struct tinywl_server *server = view->server;
  struct wlr_seat *seat = server->seat;

  seat->keyboard_state.focused_surface = NULL;
  wl_list_remove(&view->link);
  free(view);

  if (wl_list_length(&server->views) == 0) {
    return;
  }

  /* Find and focus the next view in the list */
  view = (struct tinywl_view*)(server->views.next);
  focus_view(view, view->xdg_surface->surface);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
  /* This event is raised when wlr_xdg_shell receives a new xdg surface from a
   * client, either a toplevel (application window) or popup. */
  struct tinywl_server *server = wl_container_of(listener, server, new_xdg_surface);
  struct wlr_xdg_surface *xdg_surface = data;

  if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
    return;
  }

  /* Allocate a tinywl_view for this surface */
  struct tinywl_view *view = calloc(1, sizeof(struct tinywl_view));
  view->server = server;
  view->xdg_surface = xdg_surface;

  /* Listen to the various events it can emit */
  view->map.notify = xdg_surface_map;
  wl_signal_add(&xdg_surface->events.map, &view->map);
  view->unmap.notify = xdg_surface_unmap;
  wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
  view->destroy.notify = xdg_surface_destroy;
  wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

  /* Set fullscreen size */
  struct wlr_output_layout *layout = (struct wlr_output_layout *)(view->server->output_layout);
  struct wlr_output *output = wlr_output_layout_output_at(layout, 0, 0);
  wlr_xdg_toplevel_set_size(view->xdg_surface, output->width, output->height);
  wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, true);

  /* Add it to the list of views. */
  wl_list_insert(&server->views, &view->link);
}

void maybe_drop_privileges() {
  LOGF("Running as UID/GID: [%d/%d]", getuid(), getgid());
  if (getuid() == 1000) {
    return;
  }

  LOG("Dropping privileges");
  gid_t groups[255];
  int ngroups = 255;
  int result = getgrouplist("user", 1000, groups, &ngroups);
  if (result < 0) {
    LOGFATALF("getgrouplist: Failed to get groups for user: %s", strerror(errno));
  }

  result = setgroups(ngroups, groups);
  if (result < 0) {
    LOGFATALF("getgrouplist: Failed to get groups for user: %s", strerror(errno));
  }

  if (setgid(1000) != 0)
    LOGFATALF("setuid: Unable to drop group privileges: %s", strerror(errno));
  if (setuid(1000) != 0)
    LOGFATALF("setuid: Unable to drop user privileges: %s", strerror(errno));

  LOGF("Now running as UID/GID: [%d/%d]", getuid(), getgid());
}

int main(int argc, char *argv[]) {
  struct tinywl_server server;

  log_init();
  wlr_log_init(WLR_DEBUG, NULL);

  subprogram_init();

  LOG("v128-shell starting...");
  server.wl_display = wl_display_create();
  server.backend = wlr_backend_autocreate(server.wl_display, NULL);
  server.renderer = wlr_backend_get_renderer(server.backend);
  background_init(server.renderer);
  wlr_renderer_init_wl_display(server.renderer, server.wl_display);
  wlr_compositor_create(server.wl_display, server.renderer);
  wlr_data_device_manager_create(server.wl_display);
  server.output_layout = wlr_output_layout_create();
  wl_list_init(&server.outputs);
  server.new_output.notify = server_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);

  /* Set up our list of views and the xdg-shell. The xdg-shell is a Wayland
   * protocol which is used for application windows. For more detail on
   * shells, refer to my article:
   *
   * https://drewdevault.com/2018/07/29/Wayland-shells.html
   */
  wl_list_init(&server.views);
  server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
  server.new_xdg_surface.notify = server_new_xdg_surface;
  wl_signal_add(&server.xdg_shell->events.new_surface,
                &server.new_xdg_surface);

  /*
   * Configures a seat, which is a single "seat" at which a user sits and
   * operates the computer. This conceptually includes up to one keyboard,
   * pointer, touch, and drawing tablet device. We also rig up a listener to
   * let us know when new input devices are available on the backend.
   */
  LOG("Configuring input seats...");
  wl_list_init(&server.keyboards);
  server.new_input.notify = server_new_input;
  wl_signal_add(&server.backend->events.new_input, &server.new_input);
  server.seat = wlr_seat_create(server.wl_display, "seat0");
  server.request_set_selection.notify = seat_request_set_selection;
  wl_signal_add(&server.seat->events.request_set_selection,
                &server.request_set_selection);

  /* Start the backend. This will enumerate outputs and inputs, become the DRM
   * master, etc */
  LOG("Starting backend...");
  if (!wlr_backend_start(server.backend)) {
    LOG("Starting backend failed!");
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);
    exit(1);
  }

  maybe_drop_privileges();

  /* Set the WAYLAND_DISPLAY environment variable to our socket and run the
   * startup command if requested. */
  setenv("SDL_VIDEODRIVER", "wayland", true);
  setenv("XDG_SESSION_TYPE", "wayland", true);
  setenv("QT_QPA_PLATFORM", "wayland", true);
  setenv("HOME", "/home/user", true);
  setenv("USER", "user", true);
  setenv("XDG_RUNTIME_DIR", "/run/user/1000", true);
  setenv("DISPLAY", "", true);
  subprogram_start("x128");

  /* Add a Unix socket to the Wayland display. */
  const char *socket = wl_display_add_socket_auto(server.wl_display);
  if (!socket) {
    wlr_backend_destroy(server.backend);
    return 1;
  }

  setenv("WAYLAND_DISPLAY", socket, true);
  setenv("_WAYLAND_DISPLAY", socket, true);

  /* Run the Wayland event loop. This does not return until you exit the
   * compositor. Starting the backend rigged up all of the necessary event
   * loop configuration to listen to libinput events, DRM events, generate
   * frame events at the refresh rate, and so on. */
  LOGF("Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);

  wl_display_run(server.wl_display);

  /* Once wl_display_run returns, we shut down the server. */
  LOG("Shutting down...");
  background_deinit();

  wl_display_destroy_clients(server.wl_display);
  wl_display_destroy(server.wl_display);

  return 0;
}
