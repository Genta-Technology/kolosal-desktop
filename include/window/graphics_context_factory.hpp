#pragma once

#include "wgl_context.hpp"

#include <memory>

class GraphicContextFactory {
public:
    static std::unique_ptr<GraphicsContext> createOpenGLContext()
    {
#ifdef _WIN32
        return std::make_unique<WGLContext>();
#else
        // Implement for other platforms
#endif
    }
};
