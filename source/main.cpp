#include "kolosal.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
    try 
    {
        // Create the borderless window
        g_borderlessWindow = std::make_unique<BorderlessWindow>(hInstance);

        // Initialize OpenGL
        if (!initializeOpenGL(g_borderlessWindow->handle))
            return 1;

        // Setup ImGui context
        setupImGui(g_borderlessWindow->handle);

        // Enter the main loop
        mainLoop(g_borderlessWindow->handle);

        // Cleanup resources
        cleanup();

        return 0;
    }
    catch (const std::exception& e) 
    {
        ::MessageBoxA(nullptr, e.what(), "Unhandled Exception", MB_OK | MB_ICONERROR);
        return 1;
    }
}