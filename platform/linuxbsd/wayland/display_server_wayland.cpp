/*************************************************************************/
/*  display_server_wayland.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2023 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2023 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include <poll.h>

#include "core/os/memory.h"
#include "display_server_wayland.h"

#if defined(GLES3_ENABLED)
#include "drivers/gles3/rasterizer_gles3.h"
#define _HAS_RASTERIZER
#endif

#if !defined(_HAS_RASTERIZER)
	#error "Wayland: No compatible rasterizer has been defined in the build"
#endif

#if 1
	#define DEBUG_LOG_WAYLAND(...) \
		do { \
			printf("[Wayland] "); \
			printf(__VA_ARGS__); \
		} while(0)
#else
	#define DEBUG_LOG_WAYLAND(...) ((void)0)
#endif


Vector<String> DisplayServerWayland::get_rendering_drivers() {
	Vector<String> drivers;
#if defined(GLES3_ENABLED)
	drivers.push_back("opengl3");
#endif
	return drivers;
}

DisplayServer *DisplayServerWayland::create(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error) {
	DisplayServer *display_server = memnew(DisplayServerWayland(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, r_error));
	ERR_FAIL_COND_V_MSG((display_server == nullptr) || (r_error != OK), nullptr,
		"Wayland: Failed to create Wayland display server object");

	return display_server;
}

void DisplayServerWayland::register_driver() {
	register_create_function("wayland", create, get_rendering_drivers);
}

void DisplayServerWayland::h_wl_global_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	WDisplay *display = (WDisplay *)data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		display->compositor = (struct wl_compositor *)wl_registry_bind(registry,
			name, &wl_compositor_interface, MIN(version, wl_compositor_interface.version));

	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		display->shm = (struct wl_shm *)wl_registry_bind(registry,
			name, &wl_shm_interface, MIN(version, wl_shm_interface.version));

	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		display->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(registry,
			name, &xdg_wm_base_interface, MIN(version, xdg_wm_base_interface.version));
		xdg_wm_base_add_listener(display->xdg_wm_base, &xdg_wm_base_listener, display);

	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct WScreen *screen = memnew(WScreen);
		ERR_FAIL_NULL_MSG(screen, "Wayland: Failed to allocate screen");
		screen->output_name = name;
		screen->output = (struct wl_output *)wl_registry_bind(registry,
			name, &wl_output_interface, MIN(version, wl_output_interface.version));
		display->screens.push_back(screen);
		wl_output_add_listener(screen->output, &wl_output_listener, screen);

		screen->xdg_output = zxdg_output_manager_v1_get_xdg_output(display->xdg_output_manager, screen->output);
		zxdg_output_v1_add_listener(screen->xdg_output, &xdg_output_listener, screen);

	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		display->xdg_output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(registry,
			name, &zxdg_output_manager_v1_interface, MIN(version, zxdg_output_manager_v1_interface.version));
	}
#if defined(DEV_ENABLED)
	else {
		DEBUG_LOG_WAYLAND("Global %s name %d version %d not used\n", interface, name, version);
	}
#endif
}

void DisplayServerWayland::h_wl_global_registry_remove(void *data, struct wl_registry *registry, uint32_t name) {
	WDisplay *display = (WDisplay *)data;

	for (unsigned int i = 0; i < display->screens.size(); i++) {
		if (display->screens[i]->output_name == name) {
			// Destroy objects
			zxdg_output_v1_destroy(display->screens[i]->xdg_output);
			wl_output_destroy(display->screens[i]->output);

			// Delete screen descriptor
			memdelete(display->screens[i]);
			display->screens.remove_at(i);

			return;
		}
	}

	DEBUG_LOG_WAYLAND("Ignoring removal of object 0x%08x\n", name);
}

void DisplayServerWayland::h_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	WDisplay *display = (WDisplay *)data;
	xdg_wm_base_pong(display->xdg_wm_base, serial);
	DEBUG_LOG_WAYLAND("...pong (%d)\n", serial);
}

void DisplayServerWayland::h_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	WWindow *w = (WWindow *)data;
	xdg_surface_ack_configure(w->xdg_surface, serial);
	w->pending_config = false;
}

void DisplayServerWayland::h_wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	wl_buffer_destroy(wl_buffer);
}

void DisplayServerWayland::h_wl_output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
	WScreen *screen = (WScreen *)data;

	screen->pending_update = true;
	screen->position.x = x;
	screen->position.y = y;
	screen->size_mm.width = physical_width;
	screen->size_mm.height = physical_height;
	screen->transform = transform;
}

void DisplayServerWayland::h_wl_output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	WScreen *screen = (WScreen *)data;
	if (flags & WL_OUTPUT_MODE_CURRENT) {
		screen->pending_update = true;
		screen->flags = flags;
		screen->size_px.width = width;
		screen->size_px.height = height;
		screen->refresh_mHz = refresh;
	}
}

void DisplayServerWayland::h_wl_output_done(void *data, struct wl_output *wl_output) {
	WScreen *screen = (WScreen *)data;
	screen->pending_update = false;
}

void DisplayServerWayland::h_wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor) {
	WScreen *screen = (WScreen *)data;
	screen->pending_update = true;
	screen->scale_factor = factor;
}

void DisplayServerWayland::h_wl_output_name(void *data, struct wl_output *wl_output, const char *name) {
}

void DisplayServerWayland::h_wl_output_description(void *data, struct wl_output *wl_output, const char *description) {
}

void DisplayServerWayland::h_xdg_output_logical_position(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t x, int32_t y) {
	WScreen *screen = (WScreen *)data;
	screen->pending_update = true;
	screen->logical_position.x = x;
	screen->logical_position.y = y;
}

void DisplayServerWayland::h_xdg_output_logical_size(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t width, int32_t height) {
	WScreen *screen = (WScreen *)data;
	screen->pending_update = true;
	screen->logical_size_px.width = width;
	screen->logical_size_px.height = height;
}


void DisplayServerWayland::h_xdg_output_done(void *data, struct zxdg_output_v1 *zxdg_output_v1) {
	WScreen *screen = (WScreen *)data;
	screen->pending_update = false;

	DEV_ASSERT(screen->size_mm.width && screen->size_mm.height);
	screen->dpi = (((screen->logical_size_px.width / (float(screen->size_mm.width) / 25.4f)) +
				  (screen->logical_size_px.height / (float(screen->size_mm.height) / 25.4f))) / 2.0);
}

void DisplayServerWayland::h_xdg_output_name(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *name) {
}

void DisplayServerWayland::h_xdg_output_description(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *description) {
}

void DisplayServerWayland::h_wl_surface_enter(void *data, struct wl_surface *wl_surface, struct wl_output *output) {
	WWindow *w = (WWindow *)data;
	w->outputs.push_back(output);
}

void DisplayServerWayland::h_wl_surface_leave(void *data, struct wl_surface *wl_surface, struct wl_output *output) {
	WWindow *w = (WWindow *)data;
	w->outputs.erase(output);
}

void DisplayServerWayland::h_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
	WWindow *window = (WWindow *)data;
	window->pending_config = true;

	DEBUG_LOG_WAYLAND("Wayland: xdg-toplevel Configure\n");
	DEBUG_LOG_WAYLAND("Size: %dpx,%dpx\n", width, height);
	DEBUG_LOG_WAYLAND("Size min: %dpx,%dpx\n", window->size_min.width, window->size_min.height);
	DEBUG_LOG_WAYLAND("Size max: %dpx,%dpx\n", window->size_max.width, window->size_max.height);
	DEBUG_LOG_WAYLAND("States (size:%zu):\n", states->size);

	window->mode = WINDOW_MODE_WINDOWED;
	const uint32_t *state;
	WL_ARRAY_FOR_EACH_U32(state, states) {
		DEBUG_LOG_WAYLAND("\tState: %d\n", *state);
		switch (*state) {
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			window->mode = WINDOW_MODE_MAXIMIZED;
			break;

		case XDG_TOPLEVEL_STATE_FULLSCREEN:
			window->mode = WINDOW_MODE_FULLSCREEN;
			break;

		default:
			DEBUG_LOG_WAYLAND("Unhandled window state: %d\n", *state);
			break;
		}
	}

	_window_set_size(window, Size2i(width, height));
}

void DisplayServerWayland::h_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
}

void DisplayServerWayland::h_xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
	WWindow *window = (WWindow *)data;

	if (width && height) {
		window->pending_config = true;
		window->bounds.width = width;
		window->bounds.height = height;
		print_line(vformat("Wayland: Got new bounds w:%d,h:%d", width, height));
	}
}

DisplayServer::WindowID DisplayServerWayland::_window_create(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution) {
	WWindow *w = memnew(WWindow);
	ERR_FAIL_NULL_V_MSG(w, INVALID_WINDOW_ID, "Wayland: Failed to allocate memory for window");

	w->flags = p_flags;
	w->vsync_mode = p_vsync_mode;
	w->size = p_resolution;

	w->wl_surface = wl_compositor_create_surface(display.compositor);
	if (!w->wl_surface) {
		_window_destroy(w);
		ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create surface");
	}
	wl_surface_add_listener(w->wl_surface, &wl_surface_listener, w);

	w->xdg_surface = xdg_wm_base_get_xdg_surface(display.xdg_wm_base, w->wl_surface);
	if (!w->xdg_surface) {
		_window_destroy(w);
		ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create xdg surface");
	}
	xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);

	w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
	if (!w->xdg_toplevel) {
		_window_destroy(w);
		ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create top-level xdg surface");
	}
	xdg_toplevel_add_listener(w->xdg_toplevel, &xdg_toplevel_listener, w);

	xdg_toplevel_set_title(w->xdg_toplevel, "Godot");
	w->pending_config = true;
	wl_surface_commit(w->wl_surface);

	while (w->pending_config) {
		wl_display_dispatch(display.display);
	}

#if defined(GLES3_ENABLED)
	if (display.egl_manager) {
		w->native = wl_egl_window_create(w->wl_surface, p_resolution.width, p_resolution.height);
		if (w->native == EGL_NO_SURFACE) {
			_window_destroy(w);
			ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create native EGL surface");
		}

		Error ret = display.egl_manager->window_create(w->egl_window, w->native);
		if (ret != OK) {
			_window_destroy(w);
			ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create EGL window");
		}

		RasterizerGLES3::make_current();
	}
#endif

	WindowID id = windows.find(nullptr);
	if (id < 0)
		id = windows.size();
	windows.insert(id, w);
	current_window = w;

	_window_set_mode(p_mode, w);

	return id;
}

void DisplayServerWayland::_window_destroy(WWindow *window) {
	if (window == nullptr)
		return;
	if (window->xdg_toplevel)
		xdg_toplevel_destroy(window->xdg_toplevel);
	if (window->xdg_surface)
		xdg_surface_destroy(window->xdg_surface);
	if (window->wl_surface)
		wl_surface_destroy(window->wl_surface);
	if (window->native)
		wl_egl_window_destroy(window->native);
#if defined(GLES3_ENABLED)
	if (display.egl_manager)
		display.egl_manager->window_destroy(window->egl_window);
#endif
}

DisplayServerWayland::WWindow *DisplayServerWayland::_get_window_from_id(int p_window) const {
	WWindow *w;
	if ((p_window >= 0) &&
		(p_window < windows.size()) &&
		((w = windows[p_window]) != nullptr)) {
		return w;
	} else {
		return nullptr;
	}
}

void DisplayServerWayland::_window_set_size(WWindow *window, Size2i size) {
	if ((size.width <= 0) || (size.height <= 0))
		return;

	Size2i new_size = Size2i();
	new_size.width = MIN(MAX(size.width, window->size_min.width), window->size_max.width);
	new_size.height = MIN(MAX(size.height, window->size_min.height), window->size_max.height);

	if (new_size == window->size)
		return;

	window->size = new_size;
	xdg_surface_set_window_geometry(window->xdg_surface, 0, 0, window->size.width, window->size.height);

#if defined(GLES3_ENABLED)
	if (window->native) {
		wl_egl_window_resize(window->native,
			window->size.width,
			window->size.height,
			0,0);
	}
#endif

	if (!window->rect_changed_callback.is_null()) {
		Variant rect = Rect2i(Point2i(), window->size);
		Variant *rectp = &rect;
		Variant ret;
		Callable::CallError ce;
		window->rect_changed_callback.callp((const Variant **)&rectp, 1, ret, ce);
	}
}

DisplayServerWayland::DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error) {
	r_error = ERR_UNAVAILABLE;

	// Connect to the Wayland display
	display.display = wl_display_connect(NULL);
	ERR_FAIL_NULL_MSG(display.display, "Wayland: Failed to connect to the display");

	display.fd = wl_display_get_fd(display.display);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry, &registry_listener, &display);
	wl_display_roundtrip(display.display);

	ERR_FAIL_NULL_MSG(display.compositor, "Wayland: Failed to acquire compositor");
	ERR_FAIL_NULL_MSG(display.xdg_wm_base, "Wayland: Failed to acquire xdg_wm_base");
	ERR_FAIL_NULL_MSG(display.xdg_output_manager, "Wayland: Failed to acquire xdg_output_manager");

#if defined(GLES3_ENABLED)
	if (p_rendering_driver == "opengl3") {
		display.egl_manager = memnew(EGLManager(EGL_PLATFORM_WAYLAND_KHR, display.display));
		ERR_FAIL_NULL_MSG(display.egl_manager, "Wayland: Failed to create EGL manager");

		Error ret = display.egl_manager->initialise();
		ERR_FAIL_COND_MSG(ret != OK, "Wayland: Failed initialise EGL manager");
	}
#endif

	wl_display_roundtrip(display.display);

	DEBUG_LOG_WAYLAND("Creating main window...\n");
	WindowID window_id = _window_create(p_mode, p_vsync_mode, p_flags, p_position, p_resolution);
	ERR_FAIL_COND_MSG(window_id == INVALID_WINDOW_ID, "Wayland: Failed to create main window");

	r_error = OK;
}

Vector<DisplayServer::WindowID> DisplayServerWayland::get_window_list() const {
	_THREAD_SAFE_METHOD_

	Vector<DisplayServer::WindowID> window_ids;

	for (int i = 0; i < windows.size(); i++) {
		if (windows[i] != nullptr) {
			window_ids.push_back(i);
		}
	}
	return window_ids;
}

void DisplayServerWayland::swap_buffers() {
#if defined(GLES3_ENABLED)
	if (display.egl_manager)
		display.egl_manager->swap_buffers();
#endif
}

bool DisplayServerWayland::has_feature(Feature p_feature) const {
	switch (p_feature) {
	// Supported
	case FEATURE_SWAP_BUFFERS:
		return true;

	// Not implemented yet (show debug messages for now)
	case FEATURE_SUBWINDOWS:
	case FEATURE_TOUCHSCREEN:
	case FEATURE_MOUSE:
	case FEATURE_MOUSE_WARP:
	case FEATURE_CLIPBOARD:
	case FEATURE_CURSOR_SHAPE:
	case FEATURE_CUSTOM_CURSOR_SHAPE:
	case FEATURE_IME:
	case FEATURE_WINDOW_TRANSPARENCY:
	case FEATURE_HIDPI:
	case FEATURE_ORIENTATION:
	case FEATURE_KEEP_SCREEN_ON:
	case FEATURE_CLIPBOARD_PRIMARY:
	case FEATURE_TEXT_TO_SPEECH:
		DEBUG_LOG_WAYLAND("Feature %d not implemented\n", p_feature);
		break;

	default:
		return false;
	}

	return false;
}

void DisplayServerWayland::window_set_title(const String &p_title, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w)
		xdg_toplevel_set_title(w->xdg_toplevel, p_title.utf8().get_data());
}

int DisplayServerWayland::_get_screen_id_from_window(const WWindow *window) const {
	int screen = SCREEN_UNKNOWN;

	if (!window || window->outputs.is_empty())
		return SCREEN_UNKNOWN;

	// A window can span multiple screens. Pick the "earliest" screen the
	// window has entered.
	for (unsigned int i = 0; i < display.screens.size(); i++) {
		if (display.screens[i]->output == window->outputs[0]) {
			screen = i;
			break;
		}
	}

	return screen;
}

DisplayServerWayland::WScreen *DisplayServerWayland::_get_screen_from_id(int p_screen) const {
	if (display.screens.is_empty())
		return nullptr;

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		p_screen = _get_screen_id_from_window(windows[MAIN_WINDOW_ID]);

		return p_screen == SCREEN_UNKNOWN ? nullptr : display.screens[p_screen];
	}

	if ((p_screen < 0) || (p_screen >= display.screens.size()))
		return nullptr;
	else
		return display.screens[p_screen];
}

int DisplayServerWayland::screen_get_dpi(int p_screen) const {
	_THREAD_SAFE_METHOD_

	WScreen *s = _get_screen_from_id(p_screen);
	return s ? s->dpi : INVALID_DPI;
}

int DisplayServerWayland::get_screen_count() const {
	return display.screens.size();
}

int DisplayServerWayland::get_primary_screen() const {
	// There are no generic Wayland protocols to find out the primary screen
	return SCREEN_UNKNOWN;
}

Point2i DisplayServerWayland::screen_get_position(int p_screen) const {
	_THREAD_SAFE_METHOD_

	WScreen *s = _get_screen_from_id(p_screen);

	// The API has no mechanism to return an error. For now, return (0,0) like other
	// DisplayServer implementations.
	if (!s)
		return Point2i();
	else
		return s->position;
}

Size2i DisplayServerWayland::screen_get_size(int p_screen) const {
	_THREAD_SAFE_METHOD_

	WScreen *s = _get_screen_from_id(p_screen);

	// The API has no mechanism to return an error. For now, return (0,0) like other
	// DisplayServer implementations.
	if (!s)
		return Size2i();
	else
		return s->size_px;
}

Rect2i DisplayServerWayland::screen_get_usable_rect(int p_screen) const  {
	// Wayland core protocols have no mechanism to get the usable rect from an
	// output. Similar to the X11 implementation, this function will return the
	// whole screen size. On error (invalid screen) return (0,0,0,0).
	_THREAD_SAFE_METHOD_

	WScreen *s = _get_screen_from_id(p_screen);
	return s ? Rect2i(s->logical_position, s->logical_size_px) : Rect2i();
}

float DisplayServerWayland::screen_get_refresh_rate(int p_screen) const  {
	_THREAD_SAFE_METHOD_

	WScreen *s = _get_screen_from_id(p_screen);
	return !s ? -1 : float(s->refresh_mHz) / 1000.0;
}

void DisplayServerWayland::window_set_min_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w &&
		(p_size.width > 0) &&
		(p_size.height > 0) &&
		(p_size.width <= w->size_max.width) &&
		(p_size.height <= w->size_max.height)) {
		w->size_min = p_size;
		xdg_toplevel_set_min_size(w->xdg_toplevel, p_size.width, p_size.height);
	}
}

void DisplayServerWayland::window_set_max_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w &&
		(p_size.width > 0) &&
		(p_size.height > 0) &&
		(p_size.width >= w->size_min.width) &&
		(p_size.height >= w->size_min.height)) {
		w->size_max = p_size;
		xdg_toplevel_set_max_size(w->xdg_toplevel, p_size.width, p_size.height);
	}
}

void DisplayServerWayland::window_set_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w) {
		_window_set_size(w, p_size);
	}
}

Size2i DisplayServerWayland::window_get_max_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	return w ? w->size_max : Size2i();
}

Size2i DisplayServerWayland::window_get_min_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	return w ? w->size_min : Size2i();
}

Size2i DisplayServerWayland::window_get_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	return w ? w->size : Size2i();
}

Size2i DisplayServerWayland::window_get_size_with_decorations(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerWayland::window_set_mode(WindowMode p_mode, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w)
		_window_set_mode(p_mode, w);
}

void DisplayServerWayland::_window_set_mode(WindowMode p_mode, WWindow *window) {
	if (p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
		p_mode = WINDOW_MODE_FULLSCREEN;

	if (window->mode == p_mode)
		return;

	bool is_fullscreen = window->mode == WINDOW_MODE_FULLSCREEN;
	bool is_maximized = window->mode == WINDOW_MODE_MAXIMIZED;

	switch(p_mode) {
	case WINDOW_MODE_WINDOWED:
		if (is_fullscreen)
			xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
		else if (is_maximized)
			xdg_toplevel_unset_maximized(window->xdg_toplevel);
		break;

	case WINDOW_MODE_MINIMIZED:
		xdg_toplevel_set_minimized(window->xdg_toplevel);
		break;

	case WINDOW_MODE_MAXIMIZED:
		if (is_fullscreen)
			xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
		xdg_toplevel_set_maximized(window->xdg_toplevel);
		break;

	case WINDOW_MODE_FULLSCREEN:
		xdg_toplevel_set_fullscreen(window->xdg_toplevel, nullptr);
		break;

	case WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		/* Not supported in the core Wayland protocols */
		CRASH_NOW_MSG("Wayland: Exclusive support not available");
		break;

	default:
		DEBUG_LOG_WAYLAND("Unknown window mode\n");
	}
	window->mode = p_mode;
}

DisplayServer::WindowMode DisplayServerWayland::window_get_mode(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	return w ? w->mode : WINDOW_MODE_WINDOWED;
}

int DisplayServerWayland::window_get_current_screen(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	return _get_screen_id_from_window(w);
}

void DisplayServerWayland::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	WWindow *w = _get_window_from_id(p_window);
	if (w) {
		w->rect_changed_callback = p_callable;
	}
}

DisplayServerWayland::~DisplayServerWayland() {
	if (display.compositor)
		wl_compositor_destroy(display.compositor);
	if (display.registry)
		wl_registry_destroy(display.registry);

	for (unsigned int i = 0; i < windows.size(); i++) {
		_window_destroy(windows[i]);
	}

	for (unsigned int i = 0; i < display.screens.size(); i++) {
		if (display.screens[i]->xdg_output)
			zxdg_output_v1_destroy(display.screens[i]->xdg_output);
		if (display.screens[i]->output)
			wl_output_destroy(display.screens[i]->output);
	}

	if (display.xdg_output_manager)
		zxdg_output_manager_v1_destroy(display.xdg_output_manager);

	if (display.display) {
		wl_display_flush(display.display);
		wl_display_disconnect(display.display);
	}
}

void DisplayServerWayland::process_events() {
	struct pollfd fds = {
		.fd = display.fd,
		.events = POLLIN,
		.revents = 0,
	};

	int ret = poll(&fds, 1, 0);
	if (ret) {
		_THREAD_SAFE_LOCK_
		wl_display_dispatch(display.display);
		_THREAD_SAFE_UNLOCK_
	}
}