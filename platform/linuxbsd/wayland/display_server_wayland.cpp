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

	}
#if defined(DEV_ENABLED)
	else {
		DEBUG_LOG_WAYLAND("Global %s name %d version %d not used\n", interface, name, version);
	}
#endif
}

void DisplayServerWayland::h_wl_global_registry_remove(void *data, struct wl_registry *registry, uint32_t name) {
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

DisplayServer::WindowID DisplayServerWayland::_window_create(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution) {
	WWindow *w = memnew(WWindow);
	ERR_FAIL_NULL_V_MSG(w, INVALID_WINDOW_ID, "Wayland: Failed to allocate memory for window");

	w->mode = p_mode;
	w->flags = p_flags;
	w->vsync_mode = p_vsync_mode;
	w->size = p_resolution;

	w->wl_surface = wl_compositor_create_surface(display.compositor);
	if (!w->wl_surface) {
		_window_destroy(w);
		ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Wayland: Failed to create surface");
	}

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

Size2i DisplayServerWayland::window_get_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	WWindow *w;

	if ((p_window >= 0) && (p_window < windows.size()) && ((w = windows[p_window]) != nullptr)) {
		return w->size;
	}

	return Size2i();
}

DisplayServerWayland::~DisplayServerWayland() {
	if (display.compositor)
		wl_compositor_destroy(display.compositor);
	if (display.registry)
		wl_registry_destroy(display.registry);

	for (unsigned int i = 0; i < windows.size(); i++) {
		_window_destroy(windows[i]);
	}

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