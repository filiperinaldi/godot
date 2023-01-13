/*************************************************************************/
/*  display_server_wayland.h                                             */
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

#ifndef DISPLAY_SERVER_WAYLAND_H
#define DISPLAY_SERVER_WAYLAND_H

#include <wayland-client.h>

#include "core/templates/local_vector.h"
#include "servers/display_server.h"
#include "xdg-shell.gen.h"

#if defined(GLES3_ENABLED)
#include <wayland-egl.h>
#include "egl_manager.h"
#endif


class DisplayServerWayland : public DisplayServer {
private:
	_THREAD_SAFE_CLASS_

	// Wayland handlers
	static void h_wl_global_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	static void h_wl_global_registry_remove(void *data, struct wl_registry *registry, uint32_t name);
	static void h_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
	static void h_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
	static void h_wl_buffer_release(void *data, struct wl_buffer *wl_buffer);

	// Wayland listeners
	static constexpr struct wl_registry_listener registry_listener = {
		.global = h_wl_global_registry_global,
		.global_remove = h_wl_global_registry_remove,
	};

	static constexpr struct xdg_wm_base_listener xdg_wm_base_listener = {
		.ping = h_xdg_wm_base_ping,
	};

	static constexpr struct xdg_surface_listener xdg_surface_listener = {
		.configure = h_xdg_surface_configure,
	};

	static constexpr struct wl_buffer_listener wl_buffer_listener = {
		.release = h_wl_buffer_release,
	};

	// Wayland related context
	struct WDisplay {
		int fd;
		struct wl_display *display = nullptr;
		struct wl_compositor *compositor = nullptr;
		struct wl_registry *registry = nullptr;
		struct wl_shm *shm = nullptr;
		struct xdg_wm_base *xdg_wm_base = nullptr;
#if defined(GLES3_ENABLED)
		EGLManager *egl_manager = nullptr;
#endif
	} display;

	struct WWindow {
		WindowMode mode;
		VSyncMode vsync_mode;
		uint32_t flags;
		Vector2i position;
		Vector2i resolution;
		Size2i size;

		// Wayland related
		struct wl_surface *wl_surface = nullptr;
		struct xdg_surface *xdg_surface = nullptr;
		struct xdg_toplevel *xdg_toplevel = nullptr;
		bool pending_config = true;
#if defined(GLES3_ENABLED)
		struct wl_egl_window *native = nullptr;
		EGLManager::Window egl_window;
#endif
	};

	WWindow *current_window = nullptr;
	LocalVector<WWindow *> windows;

	DisplayServer::WindowID _window_create(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution);
	void _window_destroy(WWindow *window);

public:
	static DisplayServer *create(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
	static Vector<String> get_rendering_drivers();
	static void register_driver(void);

	DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
	~DisplayServerWayland();

	String get_name() const override { return "Wayland"; }
	Vector<DisplayServer::WindowID> get_window_list() const override;
	virtual void swap_buffers() override;
	bool has_feature(Feature p_feature) const override;
	void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;
	Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	/* Not implemented yet */
	int get_screen_count() const override { WARN_PRINT_ONCE("Not implemented"); return 0; }
	int get_primary_screen() const override { WARN_PRINT_ONCE("Not implemented"); return 0; };
	Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override { WARN_PRINT_ONCE("Not implemented"); return Point2i(); }
	Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override { WARN_PRINT_ONCE("Not implemented"); return Size2i(); }
	Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override { WARN_PRINT_ONCE("Not implemented"); return Rect2i(); }
	int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override { WARN_PRINT_ONCE("Not implemented"); return 0; }
	float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override { WARN_PRINT_ONCE("Not implemented"); return 0.0; }
	WindowID get_window_at_screen_position(const Point2i &p_position) const override { WARN_PRINT_ONCE("Not implemented"); return 0; }
	void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return ObjectID(); }
	void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return 0; }
	void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Point2i(); }
	Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Point2i(); }
	void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_transient(WindowID p_window, WindowID p_parent) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Size2i(); }
	void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Size2i(); }
	void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Size2i(); }
	void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return WINDOW_MODE_WINDOWED; }
	bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return false; }
	void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return false; }
	void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return true; }
	bool can_any_window_draw() const override { WARN_PRINT_ONCE("Not implemented"); return true; }
	void process_events() override;
};

#endif // DISPLAY_SERVER_WAYLAND_H
