#include "gl_utils.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
  GLFWwindow* gWindow = 0;
  
std::string readFile(const std::string& path) {
  std::ifstream in(path.c_str());
  if (!in) {
    throw std::runtime_error("lecture impossible : " + path);
  }
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

GLuint compile(GLenum type, const std::string& src, const std::string& name) {
  GLuint shader = glCreateShader(type);
  const char* c = src.c_str();
  glShaderSource(shader, 1, &c, 0);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    throw std::runtime_error(name + " : " + log);
  }
  return shader;
}

GLuint link(GLuint a, GLuint b) {
  GLuint program = glCreateProgram();
  glAttachShader(program, a);
  if (b) {
    glAttachShader(program, b);
  }
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(program, sizeof(log), 0, log);
    throw std::runtime_error(std::string("link : ") + log);
  }
  glDeleteShader(a);
  if (b) {
    glDeleteShader(b);
  }
  return program;
}

std::string shaderPath(const char* path) {
  std::ifstream direct(path);
  if (direct) {
    return path;
  }
  return std::string("shaders") + "/" + path;
}
}

GLuint makeSSBO(GLuint binding, GLsizeiptr bytes, const void* data) {
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, data, GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
  
  return buffer;
}

void readSSBO(GLuint buffer, GLsizeiptr bytes, void* dst) {
  glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, dst);
}

GLuint ceilDiv(GLuint n, GLuint d) { return d == 0 ? 0 : (n + d - 1) / d; }

GLuint loadComputeProgram(const char* path) {
  return link(compile(GL_COMPUTE_SHADER, readFile(shaderPath(path)), path), 0);
}

GLuint loadRenderProgram(const char* vert, const char* frag) {
  return link(compile(GL_VERTEX_SHADER, readFile(shaderPath(vert)), vert),
              compile(GL_FRAGMENT_SHADER, readFile(shaderPath(frag)), frag));
}

GLFWwindow* initWindow(int width, int height, const char* title) {
  if (!glfwInit()) return 0;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  gWindow = glfwCreateWindow(width, height, title, 0, 0);
  if (!gWindow) { glfwTerminate(); return 0; }
  glfwMakeContextCurrent(gWindow);
  glfwSwapInterval(1);
  return gWindow;
}
