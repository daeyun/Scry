/**
 * @file graphics.cc
 * @author Daeyun Shin <daeyun@dshin.org>
 * @version 0.1
 * @date 2015-01-02
 * @copyright Scry is free software released under the BSD 2-Clause license.
 */
#include "graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "shader.h"
#include "shape.h"
#include "annotation.h"
#include "shader.h"
#include "shaders/blinn_shader.h"
#include "shader_object.h"
#include "config.h"
#include "framebuffer.h"
#include "io.h"
#include "gui.h"

namespace scry {

RenderParams::RenderParams() {
  // Default values
  target = glm::vec3(0, 0, 0);
  fov = 60;

  up_axis = Y;
  will_normalize = true;
  are_axes_visible = false;
  background = glm::vec4(1, 1, 1, 1);

  shader_params.ambient = glm::vec3(0.2, 0.2, 0.2);
  shader_params.shininess = 20;
  shader_params.strength = 1;

  up_angle = 0;
  el = 1;
  az = 1;
  r = 1.5;

  near = 0.1;
  far = 100;

  image_width = 800;
  image_height = 600;
  can_overwrite = false;

  num_msaa_samples = 4;

  color = arma::fvec({1, 0, 0, 1});
  is_color_forced = false;
}

/**
 * @brief
 * @param object
 * @param filename
 */
void Render(const Shape& shape, RenderParams& params) {
  bool is_off_screen = true;
  if (params.out_filename.empty()) {
    is_off_screen = false;
  }

  int window_width, window_height;
  if (is_off_screen) {
    // Invisible window
    window_width = 0;
    window_height = 0;

  } else {
    window_width = params.image_width;
    window_height = params.image_height;
  }

  GLFWwindow* window = gui::CreateWindow(window_width, window_height,
                                         config::window_title, params);

  // Support experimental drivers
  glewExperimental = true;
  GLenum glew_error = glewInit();
  if (glew_error != GLEW_OK) {
    std::cerr << glewGetErrorString(glew_error) << std::endl;
    throw std::runtime_error("Failed to open GLEW.");
  }

  Framebuffer* framebuffer;
  if (is_off_screen) {
    framebuffer = new Framebuffer(params.image_width, params.image_height,
                                  params.num_msaa_samples);
    framebuffer->Bind();
    // Required for the new framebuffer object.
    glViewport(0, 0, params.image_width, params.image_height);
    glEnable(GL_MULTISAMPLE);
  }

  glm::vec4 bg = params.background;
  glClearColor(bg.r, bg.g, bg.b, bg.a);

  // Enable depth test.
  glEnable(GL_DEPTH_TEST);

  // Accept fragment if it closer to the camera than the former one.
  glDepthFunc(GL_LESS);

  gui::render_params = &params;

  GLuint shader_id = shader::Shader(kBlinnShader);

  ShaderObject drawable_object(&shape, shader_id, nullptr);

  float min_z = arma::min(shape.v.row(2));
  Annotation annotation(min_z, params);

  do {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ComputeMatrices(params);

    annotation.draw(params);
    drawable_object.draw(params);

    glfwSwapBuffers(window);
    glfwWaitEvents();
  } while (!is_off_screen && !glfwWindowShouldClose(window));

  if (is_off_screen) {
    // Read pixel values from the framebuffer.
    size_t buffer_size = framebuffer->Size();
    uint8_t* pixels = (uint8_t*)malloc(buffer_size);
    framebuffer->ReadPixels(pixels);

    io::SaveAsPNG(params.out_filename, pixels, params.image_width,
                  params.image_height, params.can_overwrite);
    free(pixels);
    framebuffer->Unbind();

    delete framebuffer;
  }

  glDeleteProgram(shader_id);
  glfwTerminate();
}

/**
 * @brief Rotate a vector around an axis.
 * @param[in] axis Axis vector to rotate around.
 * @param[in] angle Angle in degrees. Internally converted to radians.
 * @param[out] vector Vector to rotate.
 */
void RotateVector(const glm::vec3& axis, const float angle, glm::vec3& vector) {
  vector =
      glm::vec3(glm::rotate(glm::radians(angle), axis) * glm::vec4(vector, 1));
}

/**
 * @brief
 * @param[in,out] render_params
 */
void ComputeMatrices(RenderParams& render_params) {
  float fov = glm::radians(render_params.fov);
  bool is_off_screen = !render_params.out_filename.empty();

  glm::vec3 position =
      glm::vec3(render_params.r * sin(render_params.el) * cos(render_params.az),
                render_params.r * sin(render_params.el) * sin(render_params.az),
                render_params.r * cos(render_params.el));

  glm::vec3 up =
      glm::vec3(sin(render_params.el - kPi / 2) * cos(render_params.az),
                sin(render_params.el - kPi / 2) * sin(render_params.az),
                cos(render_params.el - kPi / 2));

  glm::vec3 lookat = glm::normalize(render_params.target - position);
  RotateVector(lookat, render_params.up_angle, up);

  render_params.shader_params.projection_mat = glm::perspective(
      fov, ((float)render_params.image_width) / render_params.image_height,
      render_params.near, render_params.far);

  // Flip the image vertically. The pixels copied from the framebuffer
  // object will appear straight up.
  if (is_off_screen) render_params.shader_params.projection_mat[1] *= -1;

  render_params.shader_params.view_mat =
      glm::lookAt(position, position + lookat, up);
  render_params.shader_params.model_mat = glm::mat4(1.0);
  render_params.shader_params.eye_direction = -lookat;
}
}
