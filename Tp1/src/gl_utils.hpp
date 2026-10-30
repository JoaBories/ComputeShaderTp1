#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

GLuint makeSSBO(GLuint binding, GLsizeiptr bytes, const void* data = 0);
void readSSBO(GLuint buffer, GLsizeiptr bytes, void* dst);

GLuint ceilDiv(GLuint n, GLuint d);

GLuint loadComputeProgram(const char* path);
GLuint loadRenderProgram(const char* vert, const char* frag);

GLFWwindow* initWindow(int width, int height, const char* title);