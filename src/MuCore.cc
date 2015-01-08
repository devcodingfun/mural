/*
Copyright (c) 2015 Sean Bohan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "MuCore.h"

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"

#include "MuAppController.h"

#include <cstdio>

namespace mural {

    void errorcb(int error, const char *desc) {
        printf("GLFW error %d: %s\n", error, desc);
    }

    static void key(GLFWwindow *window, int key, int scancode, int action, int mods) {
        NVG_NOTUSED(scancode);
        NVG_NOTUSED(mods);
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }

    MuCore::MuCore():
        window(nullptr),
        renderer(nullptr),
        appController(nullptr),
        ratio(0.0f),
        defaultFont(-1),
        cpuTime(0), prevt(0), dt(0)
    {}

    MuCore::~MuCore() {
        printf("Quit Mural\n");
    }

    bool MuCore::init() {
        if (!glfwInit()) {
            printf("Failed to init GLFW.");
            return -1;
        }
        glfwSetErrorCallback(errorcb);

        // Disable window resizable
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        // OpenGL version
#ifndef _WIN32 // don't require this on win32, and works with more cards
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
#ifdef DEMO_MSAA
        glfwWindowHint(GLFW_SAMPLES, 4);
#endif

        window = glfwCreateWindow(960, 640, "Mural", NULL, NULL);
        if (!window) {
            printf("Failed to create window.");
            glfwTerminate();
            return -2;
        }

        glfwSetKeyCallback(window, key);

        glfwMakeContextCurrent(window);

        initGraph(&fpsGraph, GRAPH_RENDER_FPS, "Frame Time");
        initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
        initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");

#ifdef NANOVG_GLEW
        if (glewInit() != GLEW_OK) {
            printf("Could not init glew.\n");
            return -1;
        }
        // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
        glGetError();
#endif

    // TODO: Set MSAA from script or enable by default
#ifdef MURAL_MSAA
        renderer = nvgCreateGL3(NVG_DEBUG);
#else
        renderer = nvgCreateGL3(NVG_ANTIALIAS | NVG_DEBUG);
#endif
        if (renderer == NULL) {
            printf("Could not init nanovg.\n");
            return -1;
        }

        // Create hi-dpi FBO for hi-dpi screens.
        glfwGetWindowSize(window, &winWidth, &winHeight);
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        // Calculate pixel ration for hi-dpi devices.
        ratio = (float)fbWidth / (float)winWidth;

        // Load default font for performance rendering
        defaultFont = nvgCreateFont(renderer, "sans", "assets/Roboto-Regular.ttf");

        initGPUTimer(&gpuTimer);

        // Start timer
        glfwSetTime(0);
        prevt = glfwGetTime();
        dt = 0;

        return true;
    }

    void MuCore::start() {
        // Run loop
        while (!glfwWindowShouldClose(window)) {
            Number mx, my, t;

            float gpuTimes[3];
            int i, n;

            t = glfwGetTime();
            dt = t - prevt;
            prevt = t;

            startGPUTimer(&gpuTimer);

            glfwGetCursorPos(window, &mx, &my);
            glfwGetWindowSize(window, &winWidth, &winHeight);
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

            // Setup viewport
            glViewport(0, 0, fbWidth, fbHeight);

            // Clear screen
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            // Draw frame
            nvgBeginFrame(renderer, winWidth, winHeight, ratio);
                tickAndRender();
                // Performance graph
                renderGraph(renderer, 5, 5, &fpsGraph);
                renderGraph(renderer, 5 + 200 + 5, 5, &cpuGraph);
                if (gpuTimer.supported) {
                    renderGraph(renderer, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph);
                }
            nvgEndFrame(renderer);

            // Measure the CPU time taken excluding swap buffers
            // (as the swap may wait for GPU)
            cpuTime = glfwGetTime() - t;

            updateGraph(&fpsGraph, dt);
            updateGraph(&cpuGraph, cpuTime);

            // We may get multiple results.
            n = stopGPUTimer(&gpuTimer, gpuTimes, 3);
            for (i = 0; i < n; i++) {
                updateGraph(&gpuGraph, gpuTimes[i]);
            }

            // Swap frame
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    void MuCore::tickAndRender() {
        update();
        render();
    }

    void MuCore::update() {
        if (appController) {
            appController->update(dt);
        }
    }

    void MuCore::render() {
        if (appController) {
            appController->render(renderer);
        }
    }

    void MuCore::terminate() {
        delete appController;

        nvgDeleteGL3(renderer);
        glfwTerminate();
    }

}