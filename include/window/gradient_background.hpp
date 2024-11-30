#pragma once

#include <string>
#include <glad/glad.h>
#include <gl/gl.h>
#include <vector>
#include <iostream>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

GLuint g_shaderProgram = 0;
GLuint g_gradientTexture = 0;

const char* g_quadVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* g_quadFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gradientTexture;
uniform float uTransitionProgress;

void main()
{
    vec4 color = texture(gradientTexture, TexCoord);
    color.a *= uTransitionProgress; // Adjust the alpha based on transition progress
    FragColor = color;
}
)";

GLuint g_quadVAO = 0;
GLuint g_quadVBO = 0;
GLuint g_quadEBO = 0;

namespace GradientBackground {

	void checkShaderCompileErrors(GLuint shader, const std::string& type) {
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cerr << "| ERROR::SHADER_COMPILATION_ERROR of type: " << type << "|\n"
					<< infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}

	void checkProgramLinkErrors(GLuint program) {
		GLint success;
		GLchar infoLog[1024];
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(program, 1024, NULL, infoLog);
			std::cerr << "| ERROR::PROGRAM_LINKING_ERROR |\n"
				<< infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
		}
	}

	void generateGradientTexture(int width, int height) {
		// Delete the existing texture if it exists
		if (g_gradientTexture != 0) {
			glDeleteTextures(1, &g_gradientTexture);
		}

		// Create a new texture
		glGenTextures(1, &g_gradientTexture);
		glBindTexture(GL_TEXTURE_2D, g_gradientTexture);

		// Allocate a buffer to hold the gradient data
		std::vector<unsigned char> gradientData(width * height * 4);

		// Define the start and end colors (RGBA)
		ImVec4 colorTopLeft = ImVec4(0.05f, 0.07f, 0.12f, 1.0f);     // Dark Blue
		ImVec4 colorBottomRight = ImVec4(0.16f, 0.14f, 0.08f, 1.0f); // Dark Green

		// Generate the gradient data
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				float t_x = static_cast<float>(x) / (width - 1);
				float t_y = static_cast<float>(y) / (height - 1);
				float t = (t_x + t_y) / 2.0f; // Diagonal gradient

				ImVec4 pixelColor = ImLerp(colorTopLeft, colorBottomRight, t);

				unsigned char r = static_cast<unsigned char>(pixelColor.x * 255);
				unsigned char g = static_cast<unsigned char>(pixelColor.y * 255);
				unsigned char b = static_cast<unsigned char>(pixelColor.z * 255);
				unsigned char a = static_cast<unsigned char>(pixelColor.w * 255);

				int index = (y * width + x) * 4;
				gradientData[index + 0] = r;
				gradientData[index + 1] = g;
				gradientData[index + 2] = b;
				gradientData[index + 3] = a;
			}
		}

		// Upload the gradient data to the texture
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gradientData.data());

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Unbind the texture
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	GLuint compileShader(GLenum type, const char* source) {
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		// Check for compile errors
		checkShaderCompileErrors(shader, (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT");

		return shader;
	}

	GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
		GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

		// Link shaders into a program
		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		// Check for linking errors
		checkProgramLinkErrors(program);

		// Clean up shaders
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}

	void setupFullScreenQuad() {
		float quadVertices[] = {
			// Positions    // Texture Coords
			-1.0f,  1.0f,    0.0f, 1.0f, // Top-left
			-1.0f, -1.0f,    0.0f, 0.0f, // Bottom-left
			 1.0f, -1.0f,    1.0f, 0.0f, // Bottom-right
			 1.0f,  1.0f,    1.0f, 1.0f  // Top-right
		};

		unsigned int quadIndices[] = {
			0, 1, 2, // First triangle
			0, 2, 3  // Second triangle
		};

		glGenVertexArrays(1, &g_quadVAO);
		glGenBuffers(1, &g_quadVBO);
		glGenBuffers(1, &g_quadEBO);

		glBindVertexArray(g_quadVAO);

		// Vertex Buffer
		glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

		// Element Buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_quadEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

		// Position Attribute
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

		// Texture Coordinate Attribute
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

		glBindVertexArray(0);
	}

	void renderGradientBackground(int display_w, int display_h, float transitionProgress, float easedProgress) {
		// Set the viewport and clear the screen
		glViewport(0, 0, display_w, display_h);
		glClearColor(0, 0, 0, 0); // Clear with transparent color if blending is enabled
		glClear(GL_COLOR_BUFFER_BIT);

		// Disable depth test and face culling
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		// Render the gradient texture as background
		if (transitionProgress > 0.0f) {
			// Enable blending
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// Use the shader program
			glUseProgram(g_shaderProgram);

			// Bind the gradient texture
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, g_gradientTexture);

			// Set the sampler uniform
			glUniform1i(glGetUniformLocation(g_shaderProgram, "gradientTexture"), 0);

			// Set the transition progress uniform
			GLint locTransitionProgress = glGetUniformLocation(g_shaderProgram, "uTransitionProgress");
			glUniform1f(locTransitionProgress, easedProgress); // Use easedProgress

			// Render the full-screen quad
			glBindVertexArray(g_quadVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			// Unbind the shader program
			glUseProgram(0);

			// Disable blending if necessary
			// glDisable(GL_BLEND);
		}
	}

	void CleanUp()
	{
		if (g_gradientTexture != 0)
		{
			glDeleteTextures(1, &g_gradientTexture);
			g_gradientTexture = 0;
		}
		if (g_quadVAO != 0)
		{
			glDeleteVertexArrays(1, &g_quadVAO);
			g_quadVAO = 0;
		}
		if (g_quadVBO != 0)
		{
			glDeleteBuffers(1, &g_quadVBO);
			g_quadVBO = 0;
		}
		if (g_quadEBO != 0)
		{
			glDeleteBuffers(1, &g_quadEBO);
			g_quadEBO = 0;
		}
		if (g_shaderProgram != 0)
		{
			glDeleteProgram(g_shaderProgram);
			g_shaderProgram = 0;
		}
	}

} // namespace GradientBackground