#pragma once

#include "graphics_context.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <glad/glad.h>

class WGLContext : public GraphicsContext {
public:
    WGLContext() : deviceContext(nullptr), openglContext(nullptr), hwnd(nullptr) {}
    ~WGLContext()
    {
        if (openglContext) 
        {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(openglContext);
            openglContext = nullptr;
        }

        if (deviceContext && hwnd) 
        {
            ReleaseDC(hwnd, deviceContext);
            deviceContext = nullptr;
        }
    }

    void initialize(void* nativeWindowHandle) override
    {
        hwnd = static_cast<HWND>(nativeWindowHandle);

        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),   // Size Of This Pixel Format Descriptor
            1,                               // Version Number
            PFD_DRAW_TO_WINDOW |             // Format Must Support Window
            PFD_SUPPORT_OPENGL |             // Format Must Support OpenGL
            PFD_DOUBLEBUFFER,                // Must Support Double Buffering
            PFD_TYPE_RGBA,                   // Request An RGBA Format
            32,                              // Select Our Color Depth
            0, 0, 0, 0, 0, 0,                // Color Bits Ignored
            0,                               // No Alpha Buffer
            0,                               // Shift Bit Ignored
            0,                               // No Accumulation Buffer
            0, 0, 0, 0,                      // Accumulation Bits Ignored
            24,                              // 24Bit Z-Buffer (Depth Buffer)
            8,                               // 8Bit Stencil Buffer
            0,                               // No Auxiliary Buffer
            PFD_MAIN_PLANE,                  // Main Drawing Layer
            0,                               // Reserved
            0, 0, 0                          // Layer Masks Ignored
        };

        deviceContext = GetDC(hwnd);
        if (!deviceContext) {
            throw std::runtime_error("Failed to get device context");
        }

        int pixelFormat = ChoosePixelFormat(deviceContext, &pfd);
        if (pixelFormat == 0) {
            throw std::runtime_error("Failed to choose pixel format");
        }

        if (!SetPixelFormat(deviceContext, pixelFormat, &pfd)) {
            throw std::runtime_error("Failed to set pixel format");
        }

        openglContext = wglCreateContext(deviceContext);
        if (!openglContext) {
            throw std::runtime_error("Failed to create OpenGL context");
        }

        if (!wglMakeCurrent(deviceContext, openglContext)) {
            throw std::runtime_error("Failed to make OpenGL context current");
        }

        // Initialize GLAD or any OpenGL loader here
        if (!gladLoadGL()) {
            throw std::runtime_error("Failed to initialize GLAD");
        }
    }

    void swapBuffers() override
    {
        SwapBuffers(deviceContext);
    }

private:
    HDC deviceContext;
    HGLRC openglContext;
	HWND hwnd;
};