#pragma once

class Window;

class GraphicsContext {
public:
	virtual ~GraphicsContext() = default;
	virtual void initialize(void* nativeWindowHandle) = 0;
	virtual void swapBuffers() = 0;
};