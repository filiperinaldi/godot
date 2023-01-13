/*************************************************************************/
/*  egl_manager.h                                                        */
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
#ifndef EGL_MANAGER_H
#define EGL_MANAGER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "core/error/error_list.h"

#if !defined(EGL_VERSION_1_5)
	#error "EGL: Godot requires EGL library >= v1.5"
#endif

class EGLManager {
private:
	struct Display {
		EGLenum platform = EGL_UNKNOWN;
		void *native_display = nullptr;
		EGLDisplay display = EGL_NO_DISPLAY;
		EGLContext context = EGL_NO_CONTEXT;
		EGLConfig config = nullptr;
	} display;

public:
	struct Window {
		EGLSurface surface = EGL_NO_SURFACE;
	};

	EGLManager(EGLenum platform, void *native_display);
	~EGLManager();

	Error initialise();
	Error window_create(Window &window, void *native_surface);
	void window_destroy(Window &window);
	void swap_buffers();

private:
	Window *current_window = nullptr;
};

#endif //EGL_MANAGER_H