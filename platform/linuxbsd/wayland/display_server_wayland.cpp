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

#include "servers/rendering/dummy/rasterizer_dummy.h"

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
	drivers.push_back("dummy");
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

	RasterizerDummy::make_current();

	r_error = OK;
}

DisplayServerWayland::~DisplayServerWayland() {
	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	if (display.registry)
		wl_registry_destroy(display.registry);

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