#include "gl_utils.hpp"
#include "GLFW/glfw3.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

template <typename F>
double benchMs(F f, int warmup, int iters) {
  for (int i = 0; i < warmup; ++i) {
    f();
  }
  const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    f();
  }
  const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}

int mismatches(const std::vector<float>& a, const std::vector<float>& b) {
  int count = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) {
      ++count;
    }
  }
  return count;
}

#pragma region Exercice1
void vecAddCPU(const float* A, const float* B, float* C, int n) {
  for (int i = 0; i < n; ++i) {
    C[i] = A[i] + B[i];
  }
}

void vecADD(const float* A, const float* B, float* C, int n, GLuint shader) {
  GLuint aSSBO = makeSSBO(0, n*sizeof(float), A);
  GLuint bSSBO = makeSSBO(1, n*sizeof(float), B);
  GLuint cSSBO = makeSSBO(2, n*sizeof(float), C);
  
  glUseProgram(shader);
  glDispatchCompute(n/1024, 1, 1);
  
  glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
  readSSBO(cSSBO, n*sizeof(float), C);
  
  glDeleteBuffers(1, &aSSBO);
  glDeleteBuffers(1, &bSSBO);
  glDeleteBuffers(1, &cSSBO);
}


void exercice1()
{
  const int sizes[] = {0, 4, 1024, 2049, 99999, 524288};
  const int warmup = 10;
  const int iters = 50;
  
  GLuint shader = loadComputeProgram("vecadd.comp");
  
  std::cout << "== Exercice 1 ================= \n";
  for (int n : sizes) {
    std::vector<float> A(n), B(n), cpu(n), gpu(n, 0.f);
    for (int i = 0; i < n; ++i) {
      A[i] = float(i % 17);
      B[i] = float((i * 3) % 13);
    }
    vecAddCPU(A.data(), B.data(), cpu.data(), n);
    vecADD(A.data(), B.data(), gpu.data(), n, shader);
    const double cpuMs = benchMs([&] { vecAddCPU(A.data(), B.data(), cpu.data(), n); }, warmup, iters);
    const double gpuMs = benchMs([&] { vecADD(A.data(), B.data(), gpu.data(), n, shader); }, warmup, iters);
    std::cout << "n=" << n << " ecarts=" << mismatches(cpu, gpu)
              << " CPU " << cpuMs << " ms  vecADD " << gpuMs << " ms\n";
  }
  std::cout << "=============================== \n\n";
}
#pragma endregion 

#pragma region Exercice2
void exercice2(GLFWwindow* window, int W, int H)
{
  std::vector<unsigned> grid(static_cast<size_t>(W * H), 0u);

  std::random_device rd;
  std::mt19937 gen(rd());
  
  for (int i = 0; i < W*H; ++i)
  {
     grid[i] = gen() % 5 ? 1 : 0;
  }
  
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(grid.size() * sizeof(unsigned)), grid.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(grid.size() * sizeof(unsigned)), grid.data());
  GLuint life = loadComputeProgram("life.comp");
  GLuint display = loadRenderProgram("fullscreen.vert", "display.frag");
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  
  double ttGpuMs = 0.0;
  uint64_t ttFrame = 0;
  
  while (!glfwWindowShouldClose(window)) {
    glUseProgram(life);
    
    glUniform1ui(glGetUniformLocation(life, "width"), W);
    glUniform1ui(glGetUniformLocation(life, "height"), H);
    
    const double gpuMs = benchMs([&] 
    {  
      glDispatchCompute(ceilDiv(W, 16), ceilDiv(H, 16), 1);
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }, 0, 1);
    
    ttGpuMs += gpuMs;
    ttFrame++;
    
    std::swap(current, next);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, current);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, next);
    
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glUseProgram(display);
    glUniform1ui(glGetUniformLocation(display, "width"), static_cast<GLuint>(W));
    glUniform1ui(glGetUniformLocation(display, "height"), static_cast<GLuint>(H));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  
  std::cout << "Average gpu update time :" << ttGpuMs/ttFrame << " ms\n";
}
#pragma endregion

#pragma region Exercice3
void sumCPU(const float* v, float* sum, int n) {
  float s = 0.f;
  for (int i = 0; i < n; ++i) {
    s += v[i];
  }
  *sum = s;
}

// Pas 1, 2, 4, ... : elements espaces d'un intervalle regulier.
void reduceEntrelace(const float* v, float* sum, int n) {
  std::vector<float> x(v, v + n);
  for (int stride = 1; stride < n; stride *= 2) {
    for (int i = 0; i < n; i += stride * 2) {
      x[i] += x[i + stride];
    }
  }
  *sum = n > 0 ? x[0] : 0.f;
}

// Pas n/2, n/4, ... : l'intervalle actif est divise par deux.
void reduceMoities(const float* v, float* sum, int n) {
  std::vector<float> x(v, v + n);
  for (int stride = n / 2; stride >= 1; stride /= 2) {
    for (int i = 0; i < stride; ++i) {
      x[i] += x[i + stride];
    }
  }
  *sum = n > 0 ? x[0] : 0.f;
}

void reduce(const float* vec, float* sum, uint32_t size, GLuint shader) {
  GLuint ssbo = makeSSBO(0, size*sizeof(float), vec);
  
  glUseProgram(shader);
  glUniform1ui(glGetUniformLocation(shader, "size"), size);
  glDispatchCompute(1,1,1);
  
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  readSSBO(ssbo, sizeof(float), sum);
  
  glDeleteBuffers(1, &ssbo);
}

void reduceMult(const float* vec, float* sum, uint32_t size, GLuint shader)
{
  GLuint current = makeSSBO(0, size*sizeof(float), vec);
  GLuint next = makeSSBO(1, size*sizeof(float), 0);
  
  glUseProgram(shader);
  
  uint32_t count = size;
  while (count > 1)
  {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, current);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, next);
    
    glUniform1ui(glGetUniformLocation(shader, "size"), count);
    glDispatchCompute(ceilDiv(count, 1024) , 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    count = ceilDiv(count,1024);
    
    std::swap(current, next);
  }
  
  readSSBO(current, sizeof(float), sum);
  glDeleteBuffers(1, &next);
  glDeleteBuffers(1, &current);
}

void exercice3()
{
  const int sizes[] = {512, 1024, 16384, 65536, 262144};
  const int warmup = 10;
  const int iters = 50;

  std::cout << std::fixed << std::setprecision(4);
  std::cout << std::setw(8) << "n"
            << std::setw(14) << "|e-entrelace|"
            << std::setw(12) << "|e-moities|"
            << std::setw(12) << "|e-reduce|"
            << std::setw(16) << "|e-reduce_mult|"
            << std::setw(12) << "CPU ms"
            << std::setw(14) << "entrelace ms"
            << std::setw(12) << "moities ms"
            << std::setw(12) << "reduce ms"
            << std::setw(16) << "reduce_mult ms" << "\n";

  GLuint reduceShader = loadComputeProgram("reduce.comp");
  GLuint reduceMultShader = loadComputeProgram("reduce_mult.comp");
  
  for (int n : sizes) {
    std::vector<float> values(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
      values[i] = float(i % 17) - 8.f;
    }

    float cpu = 0.f;
    float entrelace = 0.f;
    float moities = 0.f;
    float reduceRes = 0.f;
    float reduceMultRes = 0.f;

    sumCPU(values.data(), &cpu, n);
    reduceEntrelace(values.data(), &entrelace, n);
    reduceMoities(values.data(), &moities, n);
    reduce(values.data(), &reduceRes, n, reduceShader);
    reduceMult(values.data(), &reduceMultRes, n, reduceMultShader);

    const double cpuMs = benchMs([&] { sumCPU(values.data(), &cpu, n); }, warmup, iters);
    const double entrelaceMs = benchMs([&] { reduceEntrelace(values.data(), &entrelace, n); }, warmup, iters);
    const double moitiesMs = benchMs([&] { reduceMoities(values.data(), &moities, n); }, warmup, iters);
    const double reduceMs = benchMs([&] { reduce(values.data(), &reduceRes, n, reduceShader); }, warmup, iters);
    const double reduceMultMs = benchMs([&] { reduceMult(values.data(), &reduceMultRes, n, reduceMultShader); }, warmup, iters);

    std::cout << std::setw(8) << n
              << std::setw(14) << std::fabs(cpu - entrelace)
              << std::setw(12) << std::fabs(cpu - moities)
              << std::setw(12) << std::fabs(cpu - reduceRes)
              << std::setw(16) << std::fabs(cpu - reduceMultRes)
              << std::setw(12) << cpuMs
              << std::setw(14) << entrelaceMs
              << std::setw(12) << moitiesMs
              << std::setw(12) << reduceMs
              << std::setw(16) << reduceMultMs << "\n";
  }
  
  glDeleteProgram(reduceShader);
  glDeleteProgram(reduceMultShader);
}
#pragma endregion

int main() {
  
#pragma region OpenGlInit
  const int W = 400;
  const int H = 400;
  GLFWwindow* window = initWindow(W, H, "TP1");
  
  if (!window) {
    return 1;
  }

  glewExperimental = GL_TRUE;

  if (glewInit() != GLEW_OK) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  printf("OpenGL : %s\n", glGetString(GL_VERSION));
#pragma endregion 
  
  //exercice1();
  //exercice2(window, W, H);
  exercice3();
  
  return 0;
}
