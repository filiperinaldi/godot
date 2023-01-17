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
#include "xdg-output-unstable-v1.gen.h"
#include "xdg-shell.gen.h"

#if defined(GLES3_ENABLED)
#include <wayland-egl.h>
#include "egl_manager.h"
#endif

// Documentation specifies 72 when get DPI is not supported. Using it for errors as well
#define INVALID_DPI 72

#define WL_ARRAY_FOR_EACH_U32(pos, array) \
	_WL_ARRAY_FOR_EACH(pos, array, const uint32_t *)

#define _WL_ARRAY_FOR_EACH(pos, array, type) \
	for (pos = (type)(array)->data; \
		(const uint8_t *) pos < ((const uint8_t *) (array)->data + (array)->size); \
		pos++)

class DisplayServerWayland : public DisplayServer {
private:
	_THREAD_SAFE_CLASS_

	// Wayland handlers
	static void h_wl_global_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	static void h_wl_global_registry_remove(void *data, struct wl_registry *registry, uint32_t name);
	static void h_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
	static void h_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
	static void h_wl_buffer_release(void *data, struct wl_buffer *wl_buffer);
	static void h_wl_output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform);
	static void h_wl_output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);
	static void h_wl_output_done(void *data, struct wl_output *wl_output);
	static void h_wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor);
	static void h_wl_output_name(void *data, struct wl_output *wl_output, const char *name);
	static void h_wl_output_description(void *data, struct wl_output *wl_output, const char *description);
	static void h_xdg_output_logical_position(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t x, int32_t y);
	static void h_xdg_output_logical_size(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t width, int32_t height);
	static void h_xdg_output_done(void *data, struct zxdg_output_v1 *zxdg_output_v1);
	static void h_xdg_output_name(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *name);
	static void h_xdg_output_description(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *description);
	static void h_wl_surface_enter(void *data, struct wl_surface *wl_surface, struct wl_output *output);
	static void h_wl_surface_leave(void *data, struct wl_surface *wl_surface, struct wl_output *output);
	static void h_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
	static void h_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
	static void h_xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);

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

	static constexpr struct wl_output_listener wl_output_listener = {
		.geometry = h_wl_output_geometry,
		.mode = h_wl_output_mode,
		.done = h_wl_output_done,
		.scale = h_wl_output_scale,
		.name = h_wl_output_name,
		.description = h_wl_output_description,
	};

	static constexpr struct zxdg_output_v1_listener xdg_output_listener = {
		.logical_position = h_xdg_output_logical_position,
		.logical_size = h_xdg_output_logical_size,
		.done = h_xdg_output_done,
		.name = h_xdg_output_name,
		.description = h_xdg_output_description,
	};

	static constexpr struct wl_surface_listener wl_surface_listener = {
		.enter = h_wl_surface_enter,
		.leave = h_wl_surface_leave,
	};

	static constexpr struct xdg_toplevel_listener xdg_toplevel_listener = {
		.configure = h_xdg_toplevel_configure,
		.close = h_xdg_toplevel_close,
		.configure_bounds = h_xdg_toplevel_configure_bounds,
	};

	struct WScreen {
		uint32_t output_name = 0;
		struct wl_output *output = nullptr;
		struct zxdg_output_v1 *xdg_output = nullptr;
		bool pending_update = true;
		Point2i position = Point2();
		Size2i size_mm = Size2i();
		Size2i size_px = Size2i();
		Point2i logical_position = Point2();
		Size2i logical_size_px = Size2i();
		int32_t transform = 0;
		int32_t flags = 0;
		int32_t refresh_mHz = 0;
		float scale_factor = 0;
		int dpi = INVALID_DPI;
	};

	// Wayland related context
	struct WDisplay {
		int fd;
		struct wl_display *display = nullptr;
		struct wl_compositor *compositor = nullptr;
		struct wl_registry *registry = nullptr;
		struct wl_shm *shm = nullptr;
		struct xdg_wm_base *xdg_wm_base = nullptr;
		struct zxdg_output_manager_v1 *xdg_output_manager = nullptr;
		LocalVector<WScreen *> screens;
#if defined(GLES3_ENABLED)
		EGLManager *egl_manager = nullptr;
#endif
	} display;

	struct WWindow {
		ObjectID instance_id;
		LocalVector<struct wl_output *>outputs;
		bool can_draw = true;
		WindowMode mode;
		VSyncMode vsync_mode;
		uint32_t flags;
		//Vector2i position;
		Vector2i resolution;
		Size2i size;
		Size2i size_min = Size2i(1, 1);
		Size2i size_max = Size2i(INT32_MAX, INT32_MAX);
		Size2i bounds; // Define max recommended screen size a window can occupy

		Callable rect_changed_callback;

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
	WScreen *_get_screen_from_id(int p_screen) const;
	WWindow *_get_window_from_id(int p_window) const;
	static void _window_set_size(WWindow *window, Size2i size);
	void _window_set_mode(WindowMode p_mode, WWindow *window);
	int _get_screen_id_from_window(const WWindow *window) const;

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
	int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	int get_screen_count() const override;
	int get_primary_screen() const override;
	Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;
	void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
	void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override;
	int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
	bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;
	bool can_any_window_draw() const override;
	void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	/* Not implemented yet */
	WindowID get_window_at_screen_position(const Point2i &p_position) const override { WARN_PRINT_ONCE("Not implemented"); return 0; }
	void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Point2i(); }
	Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return Point2i(); }
	void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_set_transient(WindowID p_window, WindowID p_parent) override { WARN_PRINT_ONCE("Not implemented"); return; }
	bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return false; }
	void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override { WARN_PRINT_ONCE("Not implemented"); return false; }
	void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override { WARN_PRINT_ONCE("Not implemented"); return; }
	void process_events() override;
};

#endif // DISPLAY_SERVER_WAYLAND_H
