/*************************************************************************/
/*  egl_manager.cpp                                                      */
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
#include "egl_manager.h"
#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/string/print_string.h"


EGLManager::EGLManager(EGLenum platform, void *native_display) {
	this->display.platform = platform;
	this->display.native_display = native_display;
}

EGLManager::~EGLManager() {
	eglTerminate(display.display);
}

Error EGLManager::initialise() {
	EGLBoolean ret;

	display.display = eglGetPlatformDisplay(display.platform, display.native_display, nullptr);
	ERR_FAIL_COND_V_MSG(display.display == EGL_NO_DISPLAY,
		ERR_UNAVAILABLE,
		"EGL: Requested display unavailable");

	ret = eglInitialize(display.display, nullptr, nullptr);
	ERR_FAIL_COND_V_MSG(ret == EGL_BAD_DISPLAY,
		ERR_INVALID_PARAMETER,
		"EGL: Invalid display");
	ERR_FAIL_COND_V_MSG(ret == EGL_NOT_INITIALIZED,
		FAILED,
		"EGL: Failed to initialize display");
	ERR_FAIL_COND_V_MSG(ret != EGL_TRUE,
		ERR_UNAVAILABLE,
		"EGL: EGL display unavailable");

	ret = eglBindAPI(EGL_OPENGL_API);
	ERR_FAIL_COND_V_MSG(ret != EGL_TRUE,
		FAILED,
		"EGL: Failed to bind API");

	EGLint config_size = 0;
	ret = eglGetConfigs(display.display, nullptr, 0, &config_size);
	ERR_FAIL_COND_V_MSG(ret != EGL_TRUE,
		FAILED,
		"EGL: Failed to retrieve configs");
	ERR_FAIL_COND_V_MSG(config_size == 0,
		FAILED,
		"EGL: No configs available");

	const EGLint config_attribs_min[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	const EGLint config_attribs_layered[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	const EGLint *config_attribs = OS::get_singleton()->is_layered_allowed() ? config_attribs_layered : config_attribs_min;
	EGLint config_count;
	ret = eglChooseConfig(display.display, config_attribs, &display.config, 1, &config_count);
	ERR_FAIL_COND_V_MSG(ret != EGL_TRUE,
		FAILED,
		"EGL: Failed to choose configs");
	ERR_FAIL_COND_V_MSG(config_count < 1,
		FAILED,
		"EGL: No matching configs");

	const EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_NONE
	};
	display.context = eglCreateContext(display.display, display.config, EGL_NO_CONTEXT, context_attribs);
	ERR_FAIL_COND_V_MSG(display.context == EGL_NO_CONTEXT,
		FAILED,
		"EGL: Failed to create context");

	return OK;
}

Error EGLManager::window_create(Window &window, void *native_surface) {
	window.surface = eglCreateWindowSurface(display.display, display.config, (EGLNativeWindowType)native_surface, nullptr);
	if (eglGetError() != EGL_SUCCESS) {
		ERR_FAIL_V_MSG(FAILED, "EGL: Failed to create window");
	}

	EGLBoolean ret = eglMakeCurrent(display.display, window.surface, window.surface, display.context);
	ERR_FAIL_COND_V_MSG(ret != EGL_TRUE, FAILED, "EGL: Failed to attach to new window");

	current_window = &window;
	return OK;
}

void EGLManager::window_destroy(Window &window) {
	EGLBoolean ret;

	if (window.surface == EGL_NO_SURFACE)
		return;

	ret = eglDestroySurface(display.display, window.surface);
	if (ret != EGL_TRUE)
		WARN_PRINT("EGL: Failed to destroy surface");
}

void EGLManager::swap_buffers() {
	if (current_window) {
		eglSwapBuffers(display.display, current_window->surface);
	}
}
