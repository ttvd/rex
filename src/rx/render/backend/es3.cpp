#include <SDL.h>

#include "rx/render/backend/gl.h"
#include "rx/render/backend/es3.h"

#include "rx/render/frontend/buffer.h"
#include "rx/render/frontend/target.h"
#include "rx/render/frontend/program.h"

#include "rx/core/algorithm/max.h"
#include "rx/core/math/log2.h"

#include "rx/core/profiler.h"
#include "rx/core/log.h"

#include "lib/remotery.h"

namespace rx::render::backend {

RX_LOG("render/es3", logger);

// 16MiB buffer slab size for unspecified buffer sizes
static constexpr const rx_size k_buffer_slab_size{16<<20};

// buffers
static void (GLAPIENTRYP pglGenBuffers)(GLsizei, GLuint*);
static void (GLAPIENTRYP pglDeleteBuffers)(GLsizei, const GLuint*);
static void (GLAPIENTRYP pglBufferData)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
static void (GLAPIENTRYP pglBufferSubData)(GLenum, GLintptr, GLsizeiptr, const GLvoid*);
static void (GLAPIENTRYP pglBindBuffer)(GLenum, GLuint);

// vertex arrays
static void (GLAPIENTRYP pglGenVertexArrays)(GLsizei, GLuint*);
static void (GLAPIENTRYP pglDeleteVertexArrays)(GLsizei, const GLuint*);
static void (GLAPIENTRYP pglEnableVertexAttribArray)(GLuint);
static void (GLAPIENTRYP pglVertexAttribPointer)(GLuint, GLuint, GLenum, GLboolean, GLsizei, const GLvoid*);
static void (GLAPIENTRYP pglBindVertexArray)(GLuint);

// textures
static void (GLAPIENTRYP pglGenTextures)(GLsizei, GLuint* );
static void (GLAPIENTRYP pglDeleteTextures)(GLsizei, const GLuint*);
static void (GLAPIENTRYP pglTexImage1D)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglTexImage3D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglTexSubImage1D)(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglTexSubImage3D)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*);
static void (GLAPIENTRYP pglCompressedTexImage1D)(GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const GLvoid*);
static void (GLAPIENTRYP pglCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
static void (GLAPIENTRYP pglCompressedTexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
static void (GLAPIENTRYP pglTexParameteri)(GLenum, GLenum, GLint);
static void (GLAPIENTRYP pglTexParameteriv)(GLenum, GLenum, const GLint*);
static void (GLAPIENTRYP pglTexParameterf)(GLenum, GLenum, GLfloat);
static void (GLAPIENTRYP pglBindTexture)(GLuint, GLuint);
static void (GLAPIENTRYP pglActiveTexture)(GLenum);
static void (GLAPIENTRYP pglPixelStorei)(GLenum, GLint);

// framebuffers
static void (GLAPIENTRYP pglGenFramebuffers)(GLsizei, GLuint*);
static void (GLAPIENTRYP pglDeleteFramebuffers)(GLsizei, const GLuint*);
static void (GLAPIENTRYP pglFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
static void (GLAPIENTRYP pglBindFramebuffer)(GLenum, GLuint);
static void (GLAPIENTRYP pglDrawBuffers)(GLsizei, const GLenum*);
static void (GLAPIENTRYP pglDrawBuffer)(GLenum);
static void (GLAPIENTRYP pglReadBuffer)(GLenum);
static void (GLAPIENTRYP pglBlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

// shaders and programs
static void (GLAPIENTRYP pglShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
static GLuint (GLAPIENTRYP pglCreateShader)(GLenum);
static void (GLAPIENTRYP pglDeleteShader)(GLuint);
static void (GLAPIENTRYP pglCompileShader)(GLuint);
static void (GLAPIENTRYP pglGetShaderiv)(GLuint, GLenum, GLint*);
static void (GLAPIENTRYP pglGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
static void (GLAPIENTRYP pglGetProgramiv)(GLuint, GLenum, GLint*);
static void (GLAPIENTRYP pglGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
static void (GLAPIENTRYP pglAttachShader)(GLuint, GLuint );
static void (GLAPIENTRYP pglLinkProgram)(GLuint);
static void (GLAPIENTRYP pglDetachShader)(GLuint, GLuint);
static GLuint (GLAPIENTRYP pglCreateProgram)();
static void (GLAPIENTRYP pglDeleteProgram)(GLuint);
static void (GLAPIENTRYP pglUseProgram)(GLuint);
static GLuint (GLAPIENTRYP pglGetUniformLocation)(GLuint, const GLchar*);
//static void (GLAPIENTRYP pglBindAttribLocation)(GLuint, GLuint, const char*);
//static void (GLAPIENTRYP pglBindFragDataLocation)(GLuint, GLuint, const char*);
static void (GLAPIENTRYP pglUniform1i)(GLint, GLint);
static void (GLAPIENTRYP pglUniform2iv)(GLint, GLsizei, const GLint*);
static void (GLAPIENTRYP pglUniform3iv)(GLint, GLsizei, const GLint*);
static void (GLAPIENTRYP pglUniform4iv)(GLint, GLsizei, const GLint*);
static void (GLAPIENTRYP pglUniform1fv)(GLint, GLsizei, const GLfloat*);
static void (GLAPIENTRYP pglUniform2fv)(GLint, GLsizei, const GLfloat*);
static void (GLAPIENTRYP pglUniform3fv)(GLint, GLsizei, const GLfloat*);
static void (GLAPIENTRYP pglUniform4fv)(GLint, GLsizei, const GLfloat*);
static void (GLAPIENTRYP pglUniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*);
static void (GLAPIENTRYP pglUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
static void (GLAPIENTRYP pglUniformMatrix3x4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

// state
static void (GLAPIENTRYP pglEnable)(GLenum);
static void (GLAPIENTRYP pglDisable)(GLenum);
static void (GLAPIENTRYP pglScissor)(GLint, GLint, GLsizei, GLsizei);
static void (GLAPIENTRYP pglColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
static void (GLAPIENTRYP pglBlendFuncSeparate)( GLenum, GLenum, GLenum, GLenum);
static void (GLAPIENTRYP pglDepthFunc)(GLenum);
static void (GLAPIENTRYP pglDepthMask)(GLboolean);
static void (GLAPIENTRYP pglFrontFace)(GLenum);
static void (GLAPIENTRYP pglCullFace)(GLenum);
static void (GLAPIENTRYP pglStencilMask)(GLuint);
static void (GLAPIENTRYP pglStencilFunc)(GLenum, GLint, GLuint);
static void (GLAPIENTRYP pglStencilOpSeparate)(GLenum, GLenum, GLenum, GLenum);
static void (GLAPIENTRYP pglPolygonMode)(GLenum, GLenum);
static void (GLAPIENTRYP pglViewport)(GLint, GLint, GLsizei, GLsizei);
static void (GLAPIENTRYP pglClearBufferfi)(GLenum, GLint, GLfloat, GLint);
static void (GLAPIENTRYP pglClearBufferfv)(GLenum, GLint, const GLfloat*);
static void (GLAPIENTRYP pglClearBufferiv)(GLenum, GLint, const GLint*);

// query
static void (GLAPIENTRYP pglGetIntegerv)(GLenum, GLint*);
static void (GLAPIENTRYP pglGetFloatv)(GLenum, GLfloat*);
static const GLubyte* (GLAPIENTRYP pglGetString)(GLenum);
static const GLubyte* (GLAPIENTRYP pglGetStringi)(GLenum, GLuint);

// draw calls
static void (GLAPIENTRYP pglDrawArrays)(GLenum, GLint, GLsizei);
static void (GLAPIENTRYP pglDrawElements)(GLenum, GLsizei, GLenum, const GLvoid*);

static void (GLAPIENTRYP pglFinish)(void);

namespace detail_es3 {
  struct buffer {
    buffer() {
      pglGenBuffers(2, bo);
      pglGenVertexArrays(1, &va);
    }

    ~buffer() {
      pglDeleteBuffers(2, bo);
      pglDeleteVertexArrays(1, &va);
    }

    GLuint bo[2];
    GLuint va;
    rx_size elements_size;
    rx_size vertices_size;
  };

  struct target {
    target()
      : owned{true}
    {
      memset(&draw_buffers, 0, sizeof draw_buffers);
      memset(&read_buffers, 0, sizeof read_buffers);
      pglGenFramebuffers(1, &fbo);
    }

    target(GLuint _fbo)
      : fbo{_fbo}
      , owned{false}
    {
    }

    ~target() {
      if (owned) {
        pglDeleteFramebuffers(1, &fbo);
      }
    }

    GLuint fbo;
    bool owned;
    frontend::buffers draw_buffers;
    frontend::buffers read_buffers;
  };

  struct program {
    program() {
      handle = pglCreateProgram();
    }

    ~program() {
      pglDeleteProgram(handle);
    }

    GLuint handle;
    vector<GLint> uniforms;
  };

  struct texture1D {
    texture1D() {
      pglGenTextures(1, &tex);
    }

    ~texture1D() {
      pglDeleteTextures(1, &tex);
    }

    GLuint tex;
  };

  struct texture2D {
    texture2D() {
      pglGenTextures(1, &tex);
    }

    ~texture2D() {
      pglDeleteTextures(1, &tex);
    }

    GLuint tex;
  };

  struct texture3D {
    texture3D() {
      pglGenTextures(1, &tex);
    }

    ~texture3D() {
      pglDeleteTextures(1, &tex);
    }

    GLuint tex;
  };

  struct textureCM {
    textureCM() {
      pglGenTextures(1, &tex);
    }

    ~textureCM() {
      pglDeleteTextures(1, &tex);
    }

    GLuint tex;
  };

  struct state
    : frontend::state
  {
    state(SDL_GLContext _context)
      : m_color_mask{0xff}
      , m_bound_vbo{0}
      , m_bound_ebo{0}
      , m_bound_vao{0}
      , m_bound_draw_fbo{0}
      , m_bound_read_fbo{0}
      , m_bound_program{0}
      , m_swap_chain_fbo{0}
      , m_active_texture{0}
      , m_context{_context}
    {
      memset(m_texture_units, 0, sizeof m_texture_units);

      // There's no unsigned variant of glGetIntegerv
      GLint swap_chain_fbo;
      pglGetIntegerv(GL_FRAMEBUFFER_BINDING, &swap_chain_fbo);
      m_swap_chain_fbo = static_cast<GLuint>(swap_chain_fbo);

      pglEnable(GL_CULL_FACE);
      pglCullFace(GL_BACK);
      pglFrontFace(GL_CW);

      pglDepthFunc(GL_LEQUAL);
      pglDisable(GL_MULTISAMPLE);
      pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);

      const auto vendor{reinterpret_cast<const char*>(pglGetString(GL_VENDOR))};
      const auto renderer{reinterpret_cast<const char*>(pglGetString(GL_RENDERER))};
      const auto version{reinterpret_cast<const char*>(pglGetString(GL_VERSION))};

      logger(log::level::k_info, "GL %s %s %s", vendor, version, renderer);

      GLint extensions{0};
      pglGetIntegerv(GL_NUM_EXTENSIONS, &extensions);
      for (GLint i{0}; i < extensions; i++) {
        const auto name{reinterpret_cast<const char*>(pglGetStringi(GL_EXTENSIONS, i))};
        logger(log::level::k_verbose, "extension '%s' supported", name);
      }
    }

    ~state() {
      SDL_GL_DeleteContext(m_context);
    }

    void use_enable(GLenum _thing, bool _enable) {
      if (_enable) {
        pglEnable(_thing);
      } else {
        pglDisable(_thing);
      }
    }

    void use_state(const frontend::state* _render_state) {
      profiler::cpu_sample sample{"use_state"};

      const auto& scissor{_render_state->scissor};
      const auto& blend{_render_state->blend};
      const auto& cull{_render_state->cull};
      const auto& stencil{_render_state->stencil};
      const auto& polygon{_render_state->polygon};
      const auto& depth{_render_state->depth};
      const auto& viewport{_render_state->viewport};

      if (this->scissor != scissor) {
        const auto enabled{scissor.enabled()};
        const auto offset{scissor.offset()};
        const auto size{scissor.size()};

        if (this->scissor.enabled() != enabled) {
          use_enable(GL_SCISSOR_TEST, enabled);
          this->scissor.record_enable(enabled);
        }

        if (enabled) {
          if (this->scissor.offset() != offset || this->scissor.size() != size) {
            pglScissor(offset.x, offset.y, size.w, size.h);

            this->scissor.record_offset(offset);
            this->scissor.record_size(size);
          }
        }
      }

      if (this->blend != blend) {
        const auto enabled{blend.enabled()};
        const auto color_src_factor{blend.color_src_factor()};
        const auto color_dst_factor{blend.color_dst_factor()};
        const auto alpha_src_factor{blend.alpha_src_factor()};
        const auto alpha_dst_factor{blend.alpha_dst_factor()};
        const auto write_mask{blend.write_mask()};

        if (this->blend.enabled() != enabled) {
          use_enable(GL_BLEND, enabled);
          this->blend.record_enable(enabled);
        }

        if (enabled) {
          if (this->blend.write_mask() != write_mask) {
            if (write_mask != m_color_mask) {
              const bool r{!!(write_mask & (1 << 0))};
              const bool g{!!(write_mask & (1 << 1))};
              const bool b{!!(write_mask & (1 << 2))};
              const bool a{!!(write_mask & (1 << 3))};
              pglColorMask(r, g, b, a);
              m_color_mask = write_mask;
              this->blend.record_write_mask(write_mask);
            }
          }

          if (this->blend.color_src_factor() != color_src_factor ||
              this->blend.color_dst_factor() != color_dst_factor ||
              this->blend.alpha_src_factor() != alpha_src_factor ||
              this->blend.alpha_dst_factor() != alpha_dst_factor)
          {
            pglBlendFuncSeparate(
              convert_blend_factor(color_src_factor),
              convert_blend_factor(color_dst_factor),
              convert_blend_factor(alpha_src_factor),
              convert_blend_factor(alpha_dst_factor));

            this->blend.record_color_blend_factors(color_src_factor, color_dst_factor);
            this->blend.record_alpha_blend_factors(alpha_src_factor, alpha_dst_factor);
          }
        }
      }

      if (this->depth != depth) {
        const auto test{depth.test()};
        const auto write{depth.write()};

        if (this->depth.test() != test) {
          use_enable(GL_DEPTH_TEST, test);
          this->depth.record_test(test);
        }

        if (test) {
          if (this->depth.write() != write) {
            pglDepthMask(write ? GL_TRUE : GL_FALSE);
            this->depth.record_write(write);
          }
        }
      }

      if (this->cull != cull) {
        const auto front_face{cull.front_face()};
        const auto cull_face{cull.cull_face()};
        const auto enabled{cull.enabled()};

        if (this->cull.enabled() != enabled) {
          use_enable(GL_CULL_FACE, enabled);
          this->cull.record_enable(enabled);
        }

        if (enabled) {
          if (this->cull.front_face() != front_face) {
            switch (front_face) {
            case frontend::cull_state::front_face_type::k_clock_wise:
              pglFrontFace(GL_CW);
              break;
            case frontend::cull_state::front_face_type::k_counter_clock_wise:
              pglFrontFace(GL_CCW);
              break;
            }
            this->cull.record_front_face(front_face);
          }

          if (this->cull.cull_face() != cull_face) {
            switch (cull_face) {
            case frontend::cull_state::cull_face_type::k_front:
              pglCullFace(GL_FRONT);
              break;
            case frontend::cull_state::cull_face_type::k_back:
              pglCullFace(GL_BACK);
              break;
            }
            this->cull.record_cull_face(cull_face);
          }
        }
      }

      if (this->stencil != stencil) {
        const auto enabled{stencil.enabled()};
        const auto write_mask{stencil.write_mask()};
        const auto function{stencil.function()};
        const auto reference{stencil.reference()};
        const auto mask{stencil.mask()};
        const auto front_fail_action{stencil.front_fail_action()};
        const auto front_depth_fail_action{stencil.front_depth_fail_action()};
        const auto front_depth_pass_action{stencil.front_depth_pass_action()};
        const auto back_fail_action{stencil.back_fail_action()};
        const auto back_depth_fail_action{stencil.back_depth_fail_action()};
        const auto back_depth_pass_action{stencil.back_depth_pass_action()};

        if (this->stencil.enabled() != enabled) {
          use_enable(GL_STENCIL_TEST, enabled);
          this->stencil.record_enable(enabled);
        }

        if (enabled) {
          if (this->stencil.write_mask() != write_mask) {
            pglStencilMask(write_mask);
            this->stencil.record_write_mask(write_mask);
          }

          if (this->stencil.function() != function ||
              this->stencil.reference() != reference ||
              this->stencil.mask() != mask)
          {
            pglStencilFunc(
              convert_stencil_function(function),
              static_cast<GLint>(reference),
              static_cast<GLuint>(mask));

            this->stencil.record_function(function);
            this->stencil.record_reference(reference);
            this->stencil.record_mask(mask);
          }

          if (this->stencil.front_fail_action() != front_fail_action ||
              this->stencil.front_depth_fail_action() != front_depth_fail_action ||
              this->stencil.front_depth_pass_action() != front_depth_pass_action)
          {
            pglStencilOpSeparate(
              GL_FRONT,
              convert_stencil_operation(front_fail_action),
              convert_stencil_operation(front_depth_fail_action),
              convert_stencil_operation(front_depth_pass_action));

            this->stencil.record_front_fail_action(front_fail_action);
            this->stencil.record_front_depth_fail_action(front_depth_fail_action);
            this->stencil.record_front_depth_pass_action(front_depth_pass_action);
          }

          if (this->stencil.back_fail_action() != back_fail_action ||
              this->stencil.back_depth_fail_action() != back_depth_fail_action ||
              this->stencil.back_depth_pass_action() != back_depth_pass_action)
          {
            pglStencilOpSeparate(
              GL_BACK,
              convert_stencil_operation(back_fail_action),
              convert_stencil_operation(back_depth_fail_action),
              convert_stencil_operation(back_depth_pass_action));

            this->stencil.record_back_fail_action(back_fail_action);
            this->stencil.record_back_depth_fail_action(back_depth_fail_action);
            this->stencil.record_back_depth_pass_action(back_depth_pass_action);
          }
        }
      }

      if (this->polygon != polygon) {
        const auto mode{polygon.mode()};
        pglPolygonMode(GL_FRONT_AND_BACK, convert_polygon_mode(mode));
        this->polygon.record_mode(mode);
      }

      if (this->viewport != viewport) {
        const auto& offset{viewport.offset().cast<GLuint>()};
        const auto& dimensions{viewport.dimensions().cast<GLsizei>()};
        pglViewport(offset.x, offset.y, dimensions.w, dimensions.h);
        this->viewport.record_offset(viewport.offset());
        this->viewport.record_dimensions(viewport.dimensions());
      }

      // flush all changes to this for updated hash
      flush();
    }

    void use_draw_target(frontend::target* _render_target, const frontend::buffers* _draw_buffers) {
      profiler::cpu_sample sample{"use_draw_target"};

      auto this_target{reinterpret_cast<target*>(_render_target + 1)};
      if (m_bound_draw_fbo != this_target->fbo) {
        pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, this_target->fbo);
        m_bound_draw_fbo = this_target->fbo;
      }

      // Changing draw buffers?
      if (_draw_buffers && !_render_target->is_swapchain()) {
        // The draw buffers changed.
        if (this_target->draw_buffers != *_draw_buffers) {
          if (_draw_buffers->index == 0) {
            pglDrawBuffer(GL_NONE);
          } else {
            vector<GLenum> draw_buffers;
            for (rx_u8 i{0}; i < _draw_buffers->index; i++) {
              draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + _draw_buffers->elements[i]);
            }
            pglDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
          }
          this_target->draw_buffers = *_draw_buffers;
        }
      }
    }

    void use_read_target(frontend::target* _render_target, const frontend::buffers* _read_buffers) {
      profiler::cpu_sample sample{"use_read_target"};

      auto this_target{reinterpret_cast<target*>(_render_target + 1)};
      if (m_bound_read_fbo != this_target->fbo) {
        pglBindFramebuffer(GL_READ_FRAMEBUFFER, this_target->fbo);
        m_bound_read_fbo = this_target->fbo;
      }

      // Changing read buffer?
      if (_read_buffers && !_render_target->is_swapchain()) {
        // The read buffer changed.
        if (this_target->read_buffers != *_read_buffers) {
          if (_read_buffers->index == 0) {
            pglReadBuffer(GL_NONE);
          } else {
            pglReadBuffer(GL_COLOR_ATTACHMENT0 + _read_buffers->elements[_read_buffers->index - 1]);
          }
        }
        this_target->read_buffers = *_read_buffers;
      }
    }

    void use_program(const frontend::program* _render_program) {
      profiler::cpu_sample sample{"use_program"};

      const auto this_program{reinterpret_cast<const program*>(_render_program + 1)};
      if (this_program->handle != m_bound_program) {
        pglUseProgram(this_program->handle);
        m_bound_program = this_program->handle;
      }
    }

    void use_buffer(const frontend::buffer* _render_buffer) {
      profiler::cpu_sample sample{"use_buffer"};

      const auto this_buffer{reinterpret_cast<const buffer*>(_render_buffer + 1)};
      if (this_buffer->va != m_bound_vao) {
        pglBindVertexArray(this_buffer->va);
        m_bound_vao = this_buffer->va;
      }
    }

    void use_vbo(GLuint _vbo) {
      profiler::cpu_sample sample{"use_vbo"};

      if (m_bound_vbo != _vbo) {
        pglBindBuffer(GL_ARRAY_BUFFER, _vbo);
        m_bound_vbo = _vbo;
      }
    }

    void use_ebo(GLuint _ebo) {
      profiler::cpu_sample sample{"use_ebo"};

      if (m_bound_ebo != _ebo) {
        pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        m_bound_ebo = _ebo;
      }
    }

    struct texture_unit {
      GLuint texture1D;
      GLuint texture2D;
      GLuint texture3D;
      GLuint textureCM;
    };

    template<GLuint texture_unit::*name, typename Bt, typename Ft>
    void use_texture_template(GLenum _type, const Ft* _render_texture) {
      profiler::cpu_sample sample{"use_texture"};

      const auto this_texture{reinterpret_cast<const Bt*>(_render_texture + 1)};
      auto& texture_unit{m_texture_units[m_active_texture]};
      if (texture_unit.*name != this_texture->tex) {
        texture_unit.*name = this_texture->tex;
        pglBindTexture(_type, this_texture->tex);
      }
    }

    template<GLuint texture_unit::*name, typename Bt, typename Ft>
    void use_active_texture_template(GLenum _type, const Ft* _render_texture, rx_size _unit) {
      const auto this_texture{reinterpret_cast<const Bt*>(_render_texture + 1)};
      auto& texture_unit{m_texture_units[_unit]};
      if (texture_unit.*name != this_texture->tex) {
        if (m_active_texture != _unit) {
          pglActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + _unit));
          m_active_texture = _unit;
        }
        texture_unit.*name = this_texture->tex;
        pglBindTexture(_type, this_texture->tex);
      }
    }

    template<typename Ft, typename Bt, GLuint texture_unit::*name>
    void invalidate_texture_template(const Ft* _render_texture) {
      const auto this_texture{reinterpret_cast<const Bt*>(_render_texture + 1)};
      for (auto& texture_unit : m_texture_units) {
        if (texture_unit.*name == this_texture->tex) {
          texture_unit.*name = 0;
        }
      }
    }

    void use_active_texture(const frontend::texture1D* _render_texture, rx_size _unit) {
      use_active_texture_template<&texture_unit::texture1D, detail_es3::texture1D>(GL_TEXTURE_1D, _render_texture, _unit);
    }

    void use_active_texture(const frontend::texture2D* _render_texture, rx_size _unit) {
      use_active_texture_template<&texture_unit::texture2D, detail_es3::texture2D>(GL_TEXTURE_2D, _render_texture, _unit);
    }

    void use_active_texture(const frontend::texture3D* _render_texture, rx_size _unit) {
      use_active_texture_template<&texture_unit::texture1D, detail_es3::texture1D>(GL_TEXTURE_1D, _render_texture, _unit);
    }

    void use_active_texture(const frontend::textureCM* _render_texture, rx_size _unit) {
      use_active_texture_template<&texture_unit::textureCM, detail_es3::textureCM>(GL_TEXTURE_CUBE_MAP, _render_texture, _unit);
    }

    void use_texture(const frontend::texture1D* _render_texture) {
      use_texture_template<&texture_unit::texture1D, detail_es3::texture1D>(GL_TEXTURE_1D, _render_texture);
    }

    void use_texture(const frontend::texture2D* _render_texture) {
      use_texture_template<&texture_unit::texture2D, detail_es3::texture2D>(GL_TEXTURE_2D, _render_texture);
    }

    void use_texture(const frontend::texture3D* _render_texture) {
      use_texture_template<&texture_unit::texture3D, detail_es3::texture3D>(GL_TEXTURE_3D, _render_texture);
    }

    void use_texture(const frontend::textureCM* _render_texture) {
      use_texture_template<&texture_unit::textureCM, detail_es3::textureCM>(GL_TEXTURE_CUBE_MAP, _render_texture);
    }

    void invalidate_texture(const frontend::texture1D* _render_texture) {
      invalidate_texture_template<frontend::texture1D, texture1D, &texture_unit::texture1D>(_render_texture);
    }

    void invalidate_texture(const frontend::texture2D* _render_texture) {
      invalidate_texture_template<frontend::texture2D, texture2D, &texture_unit::texture2D>(_render_texture);
    }

    void invalidate_texture(const frontend::texture3D* _render_texture) {
      invalidate_texture_template<frontend::texture3D, texture3D, &texture_unit::texture3D>(_render_texture);
    }

    void invalidate_texture(const frontend::textureCM* _render_texture) {
      invalidate_texture_template<frontend::textureCM, textureCM, &texture_unit::textureCM>(_render_texture);
    }

    rx_u8 m_color_mask;

    GLuint m_bound_vbo;
    GLuint m_bound_ebo;
    GLuint m_bound_vao;
    GLuint m_bound_draw_fbo;
    GLuint m_bound_read_fbo;
    GLuint m_bound_program;

    GLuint m_swap_chain_fbo;
    texture_unit m_texture_units[frontend::draw_command::k_max_textures];
    rx_size m_active_texture;

    SDL_GLContext m_context;
  };
};

template<typename F>
static void fetch(const char* _name, F& function_) {
  auto address{SDL_GL_GetProcAddress(_name)};
  logger(log::level::k_verbose, "loaded %08p '%s'", address, _name);
  *reinterpret_cast<void**>(&function_) = address;
}

static constexpr const char* inout_to_string(frontend::shader::inout_type _type) {
  switch (_type) {
  case frontend::shader::inout_type::k_mat4x4f:
    return "mat4";
  case frontend::shader::inout_type::k_mat3x3f:
    return "mat3";
  case frontend::shader::inout_type::k_vec2f:
    return "vec2f";
  case frontend::shader::inout_type::k_vec3f:
    return "vec3f";
  case frontend::shader::inout_type::k_vec4f:
    return "vec4f";
  case frontend::shader::inout_type::k_vec2i:
    return "vec2i";
  case frontend::shader::inout_type::k_vec3i:
    return "vec3i";
  case frontend::shader::inout_type::k_vec4i:
    return "vec4i";
  case frontend::shader::inout_type::k_vec4b:
    return "vec4b";
  case frontend::shader::inout_type::k_float:
    return "float";
  }
  return nullptr;
}

static constexpr const char* uniform_to_string(frontend::uniform::type _type) {
  switch (_type) {
  case frontend::uniform::type::k_sampler1D:
    return "rx_sampler1D";
  case frontend::uniform::type::k_sampler2D:
    return "rx_sampler2D";
  case frontend::uniform::type::k_sampler3D:
    return "rx_sampler3D";
  case frontend::uniform::type::k_samplerCM:
    return "rx_samplerCM";
  case frontend::uniform::type::k_bool:
    return "bool";
  case frontend::uniform::type::k_int:
    return "int";
  case frontend::uniform::type::k_float:
    return "float";
  case frontend::uniform::type::k_vec2i:
    return "vec2i";
  case frontend::uniform::type::k_vec3i:
    return "vec3i";
  case frontend::uniform::type::k_vec4i:
    return "vec4i";
  case frontend::uniform::type::k_vec2f:
    return "vec2f";
  case frontend::uniform::type::k_vec3f:
    return "vec3f";
  case frontend::uniform::type::k_vec4f:
    return "vec4f";
  case frontend::uniform::type::k_mat4x4f:
    return "mat4x4f";
  case frontend::uniform::type::k_mat3x3f:
    return "mat3x3f";
  case frontend::uniform::type::k_bonesf:
    return "bonesf";
  }
  return nullptr;
}

static GLuint compile_shader(const vector<frontend::uniform>& _uniforms,
  const frontend::shader& _shader)
{
  // emit prelude to every shader
  static constexpr const char* k_prelude{
    "#version 330 core\n"
    "#extension GL_OES_standard_derivatives : enable\n"
    "#define vec2f vec2\n"
    "#define vec3f vec3\n"
    "#define vec4f vec4\n"
    "#define vec2i ivec2\n"
    "#define vec3i ivec3\n"
    "#define vec4i ivec4\n"
    "#define vec4b vec4\n"
    "#define mat3x3f mat3\n"
    "#define mat4x4f mat4\n"
    "#define mat3x4f mat3x4\n"
    "#define bonesf mat3x4f[80]\n"
    "#define rx_sampler1D sampler1D\n"
    "#define rx_sampler2D sampler2D\n"
    "#define rx_sampler3D sampler3D\n"
    "#define rx_samplerCM samplerCube\n"
    "#define rx_texture1D texture\n"
    "#define rx_texture2D texture\n"
    "#define rx_texture3D texture\n"
    "#define rx_textureCM texture\n"
    "#define rx_texture1DLod textureLod\n"
    "#define rx_texture2DLod textureLod\n"
    "#define rx_texture3DLod textureLod\n"
    "#define rx_textureCMLod textureLod\n"
    "#define rx_position gl_Position\n"
    "#define rx_point_size gl_PointSize\n"
  };

  string contents{k_prelude};

  GLenum type{0};
  switch (_shader.kind) {
  case frontend::shader::type::k_vertex:
    type = GL_VERTEX_SHADER;
    // emit vertex attributes inputs
    _shader.inputs.each_pair([&](const string& _name, const frontend::shader::inout& _inout) {
      contents.append(string::format("layout(location = %zu) in %s %s;\n", _inout.index, inout_to_string(_inout.kind), _name));
    });
    // emit vertex outputs
    _shader.outputs.each_pair([&](const string& _name, const frontend::shader::inout& _inout) {
      contents.append(string::format("out %s %s;\n", inout_to_string(_inout.kind), _name));
    });
    break;
  case frontend::shader::type::k_fragment:
    type = GL_FRAGMENT_SHADER;
    // emit fragment inputs
    _shader.inputs.each_pair([&](const string& _name, const frontend::shader::inout& _inout) {
      contents.append(string::format("in %s %s;\n", inout_to_string(_inout.kind), _name));
    });
    // emit fragment outputs
    _shader.outputs.each_pair([&](const string& _name, const frontend::shader::inout& _inout) {
      contents.append(string::format("layout(location = %d) out %s %s;\n", _inout.index, inout_to_string(_inout.kind), _name));
    });
    break;
  }

  // emit uniforms
  _uniforms.each_fwd([&](const frontend::uniform& _uniform) {
    // Don't emit padding uniforms.
    if (!_uniform.is_padding()) {
      contents.append(string::format("uniform %s %s;\n", uniform_to_string(_uniform.kind()), _uniform.name()));
    }
  });

  // to get good diagnostics
  contents.append("#line 0\n");

  // append the user shader source now
  contents.append(_shader.source);

  const GLchar* data{static_cast<const GLchar*>(contents.data())};
  const GLint size{static_cast<GLint>(contents.size())};

  GLuint handle{pglCreateShader(type)};
  pglShaderSource(handle, 1, &data, &size);
  pglCompileShader(handle);

  GLint status{0};
  pglGetShaderiv(handle, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    GLint log_size{0};
    pglGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_size);

    logger(log::level::k_error, "failed compiling shader");

    if (log_size) {
      vector<char> error_log{&memory::g_system_allocator, static_cast<rx_size>(log_size)};
      pglGetShaderInfoLog(handle, log_size, &log_size, error_log.data());
      logger(log::level::k_error, "\n%s\n%s", error_log.data(), contents);
    }

    pglDeleteShader(handle);
    return 0;
  }

  return handle;
}

allocation_info es3::query_allocation_info() const {
  allocation_info info;
  info.buffer_size = sizeof(detail_es3::buffer);
  info.target_size = sizeof(detail_es3::target);
  info.program_size = sizeof(detail_es3::program);
  info.texture1D_size = sizeof(detail_es3::texture1D);
  info.texture2D_size = sizeof(detail_es3::texture2D);
  info.texture3D_size = sizeof(detail_es3::texture3D);
  info.textureCM_size = sizeof(detail_es3::textureCM);
  return info;
}

device_info es3::query_device_info() const {
  return {
    reinterpret_cast<const char*>(pglGetString(GL_VENDOR)),
    reinterpret_cast<const char*>(pglGetString(GL_RENDERER)),
    reinterpret_cast<const char*>(pglGetString(GL_VERSION))
  };
}

es3::es3(memory::allocator* _allocator, void* _data)
  : m_allocator{_allocator}
  , m_data{_data}
{
}

es3::~es3() {
  m_allocator->destroy<detail_es3::state>(m_impl);
}

bool es3::init() {
  SDL_GLContext context{SDL_GL_CreateContext(reinterpret_cast<SDL_Window*>(m_data))};
  if (!context) {
    return false;
  }

  // buffers
  fetch("glGenBuffers", pglGenBuffers);
  fetch("glDeleteBuffers", pglDeleteBuffers);
  fetch("glBufferData", pglBufferData);
  fetch("glBufferSubData", pglBufferSubData);
  fetch("glBindBuffer", pglBindBuffer);

  // vertex arrays
  fetch("glGenVertexArrays", pglGenVertexArrays);
  fetch("glDeleteVertexArrays", pglDeleteVertexArrays);
  fetch("glEnableVertexAttribArray", pglEnableVertexAttribArray);
  fetch("glVertexAttribPointer", pglVertexAttribPointer);
  fetch("glBindVertexArray", pglBindVertexArray);

  // textures
  fetch("glGenTextures", pglGenTextures);
  fetch("glDeleteTextures", pglDeleteTextures);
  fetch("glTexImage1D", pglTexImage1D);
  fetch("glTexImage2D", pglTexImage2D);
  fetch("glTexImage3D", pglTexImage3D);
  fetch("glTexSubImage1D", pglTexSubImage1D);
  fetch("glTexSubImage2D", pglTexSubImage2D);
  fetch("glTexSubImage3D", pglTexSubImage3D);
  fetch("glCompressedTexImage1D", pglCompressedTexImage1D);
  fetch("glCompressedTexImage2D", pglCompressedTexImage2D);
  fetch("glCompressedTexImage3D", pglCompressedTexImage3D);
  fetch("glTexParameteri", pglTexParameteri);
  fetch("glTexParameteriv", pglTexParameteriv);
  fetch("glTexParameterf", pglTexParameterf);
  fetch("glBindTexture", pglBindTexture);
  fetch("glActiveTexture", pglActiveTexture);
  fetch("glPixelStorei", pglPixelStorei);

  // frame buffers
  fetch("glGenFramebuffers", pglGenFramebuffers);
  fetch("glDeleteFramebuffers", pglDeleteFramebuffers);
  fetch("glFramebufferTexture2D", pglFramebufferTexture2D);
  fetch("glBindFramebuffer", pglBindFramebuffer);
  fetch("glDrawBuffers", pglDrawBuffers);
  fetch("glDrawBuffer", pglDrawBuffer);
  fetch("glReadBuffer", pglReadBuffer);
  fetch("glBlitFramebuffer", pglBlitFramebuffer);
  fetch("glClearBufferfv", pglClearBufferfv);
  fetch("glClearBufferiv", pglClearBufferiv);
  fetch("glClearBufferfi", pglClearBufferfi);

  // shaders and programs
  fetch("glShaderSource", pglShaderSource);
  fetch("glCreateShader", pglCreateShader);
  fetch("glDeleteShader", pglDeleteShader);
  fetch("glCompileShader", pglCompileShader);
  fetch("glGetShaderiv", pglGetShaderiv);
  fetch("glGetShaderInfoLog", pglGetShaderInfoLog);
  fetch("glGetProgramiv", pglGetProgramiv);
  fetch("glGetProgramInfoLog", pglGetProgramInfoLog);
  fetch("glAttachShader", pglAttachShader);
  fetch("glLinkProgram", pglLinkProgram);
  fetch("glDetachShader", pglDetachShader);
  fetch("glCreateProgram", pglCreateProgram);
  fetch("glDeleteProgram", pglDeleteProgram);
  fetch("glUseProgram", pglUseProgram);
  fetch("glGetUniformLocation", pglGetUniformLocation);
  fetch("glUniform1i", pglUniform1i);
  fetch("glUniform2iv", pglUniform2iv);
  fetch("glUniform3iv", pglUniform3iv);
  fetch("glUniform4iv", pglUniform4iv);
  fetch("glUniform1fv", pglUniform1fv);
  fetch("glUniform2fv", pglUniform2fv);
  fetch("glUniform3fv", pglUniform3fv);
  fetch("glUniform4fv", pglUniform4fv);
  fetch("glUniformMatrix3fv", pglUniformMatrix3fv);
  fetch("glUniformMatrix4fv", pglUniformMatrix4fv);
  fetch("glUniformMatrix3x4fv", pglUniformMatrix3x4fv);

  // state
  fetch("glEnable", pglEnable);
  fetch("glDisable", pglDisable);
  fetch("glScissor", pglScissor);
  fetch("glColorMask", pglColorMask);
  fetch("glBlendFuncSeparate", pglBlendFuncSeparate);
  fetch("glDepthFunc", pglDepthFunc);
  fetch("glDepthMask", pglDepthMask);
  fetch("glFrontFace", pglFrontFace);
  fetch("glCullFace", pglCullFace);
  fetch("glStencilMask", pglStencilMask);
  fetch("glStencilFunc", pglStencilFunc);
  fetch("glStencilOpSeparate", pglStencilOpSeparate);
  fetch("glPolygonMode", pglPolygonMode);
  fetch("glViewport", pglViewport);

  // query
  fetch("glGetIntegerv", pglGetIntegerv);
  fetch("glGetFloatv", pglGetFloatv);
  fetch("glGetString", pglGetString);
  fetch("glGetStringi", pglGetStringi);

  // draw calls
  fetch("glDrawArrays", pglDrawArrays);
  fetch("glDrawElements", pglDrawElements);

  fetch("glFinish", pglFinish);

  m_impl = m_allocator->create<detail_es3::state>(context);

  return true;
}

void es3::process(const vector<rx_byte*>& _commands) {
  _commands.each_fwd([this](rx_byte* _command) {
    process(_command);
  });
}

void es3::process(rx_byte* _command) {
  profiler::cpu_sample sample{"es3::process"};

  auto state{reinterpret_cast<detail_es3::state*>(m_impl)};
  auto header{reinterpret_cast<frontend::command_header*>(_command)};
  switch (header->type) {
  case frontend::command_type::k_resource_allocate:
    {
      const auto resource{reinterpret_cast<const frontend::resource_command*>(header + 1)};
      switch (resource->kind) {
      case frontend::resource_command::type::k_buffer:
        utility::construct<detail_es3::buffer>(resource->as_buffer + 1);
        break;
      case frontend::resource_command::type::k_target:
        {
          const auto render_target{resource->as_target};
          if (render_target->is_swapchain()) {
            utility::construct<detail_es3::target>(resource->as_target + 1, state->m_swap_chain_fbo);
          } else {
            utility::construct<detail_es3::target>(resource->as_target + 1);
          }
        }
        break;
      case frontend::resource_command::type::k_program:
        utility::construct<detail_es3::program>(resource->as_program + 1);
        break;
      case frontend::resource_command::type::k_texture1D:
        utility::construct<detail_es3::texture1D>(resource->as_texture1D + 1);
        break;
      case frontend::resource_command::type::k_texture2D:
        if (resource->as_texture2D->is_swapchain()) {
          break;
        }
        utility::construct<detail_es3::texture2D>(resource->as_texture2D + 1);
        break;
      case frontend::resource_command::type::k_texture3D:
        utility::construct<detail_es3::texture3D>(resource->as_texture3D + 1);
        break;
      case frontend::resource_command::type::k_textureCM:
        utility::construct<detail_es3::textureCM>(resource->as_textureCM + 1);
        break;
      }
    }
    break;
  case frontend::command_type::k_resource_destroy:
    {
      const auto resource{reinterpret_cast<const frontend::resource_command*>(header + 1)};
      switch (resource->kind) {
      case frontend::resource_command::type::k_buffer:
        {
          auto buffer{reinterpret_cast<detail_es3::buffer*>(resource->as_buffer + 1)};
          if (state->m_bound_vbo == buffer->bo[0]) {
            state->m_bound_vbo = 0;
          }
          if (state->m_bound_ebo == buffer->bo[1]) {
            state->m_bound_ebo = 0;
          }
          if (state->m_bound_vao == buffer->va) {
            state->m_bound_vao = 0;
          }
          utility::destruct<detail_es3::buffer>(resource->as_buffer + 1);
        }
        break;
      case frontend::resource_command::type::k_target:
        {
          auto target{reinterpret_cast<detail_es3::target*>(resource->as_target + 1)};
          if (state->m_bound_draw_fbo == target->fbo) {
            state->m_bound_draw_fbo = 0;
          }
          if (state->m_bound_read_fbo == target->fbo) {
            state->m_bound_read_fbo = 0;
          }
          utility::destruct<detail_es3::target>(resource->as_target + 1);
        }
        break;
      case frontend::resource_command::type::k_program:
        utility::destruct<detail_es3::program>(resource->as_program + 1);
        break;
      case frontend::resource_command::type::k_texture1D:
        state->invalidate_texture(resource->as_texture1D);
        utility::destruct<detail_es3::texture1D>(resource->as_texture1D + 1);
        break;
      case frontend::resource_command::type::k_texture2D:
        if (resource->as_texture2D->is_swapchain()) {
          break;
        }
        state->invalidate_texture(resource->as_texture2D);
        utility::destruct<detail_es3::texture2D>(resource->as_texture2D + 1);
        break;
      case frontend::resource_command::type::k_texture3D:
        state->invalidate_texture(resource->as_texture3D);
        utility::destruct<detail_es3::texture3D>(resource->as_texture3D + 1);
        break;
      case frontend::resource_command::type::k_textureCM:
        state->invalidate_texture(resource->as_textureCM);
        utility::destruct<detail_es3::textureCM>(resource->as_textureCM + 1);
        break;
      }
    }
    break;
  case frontend::command_type::k_resource_construct:
    {
      const auto resource{reinterpret_cast<const frontend::resource_command*>(header + 1)};
      switch (resource->kind) {
      case frontend::resource_command::type::k_buffer:
        {
          const auto render_buffer{resource->as_buffer};
          auto buffer{reinterpret_cast<detail_es3::buffer*>(render_buffer + 1)};
          const auto& vertices{render_buffer->vertices()};
          const auto& elements{render_buffer->elements()};
          const auto type{render_buffer->kind() == frontend::buffer::type::k_dynamic
            ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW};

          state->use_buffer(render_buffer);

          state->use_vbo(buffer->bo[0]);
          state->use_ebo(buffer->bo[1]);

          if (vertices.size()) {
            pglBufferData(GL_ARRAY_BUFFER, vertices.size(), vertices.data(), type);
            buffer->vertices_size = vertices.size();
          } else {
            pglBufferData(GL_ARRAY_BUFFER, k_buffer_slab_size, nullptr, type);
            buffer->vertices_size = k_buffer_slab_size;
          }

          if (elements.size()) {
            pglBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size(), elements.data(), type);
            buffer->elements_size = elements.size();
          } else {
            pglBufferData(GL_ELEMENT_ARRAY_BUFFER, k_buffer_slab_size, nullptr, type);
            buffer->elements_size = k_buffer_slab_size;
          }

          const auto& attributes{render_buffer->attributes()};
          for (rx_size i{0}; i < attributes.size(); i++) {
            const auto& attribute{attributes[i]};
            const auto index{static_cast<GLuint>(i)};
            pglEnableVertexAttribArray(index);

            switch (attribute.kind) {
            case frontend::buffer::attribute::type::k_f32:
              pglVertexAttribPointer(
                index,
                static_cast<GLsizei>(attribute.count),
                GL_FLOAT,
                GL_FALSE,
                render_buffer->stride(),
                reinterpret_cast<const GLvoid*>(attribute.offset));
              break;
            case frontend::buffer::attribute::type::k_u8:
              pglVertexAttribPointer(
                index,
                static_cast<GLsizei>(attribute.count),
                GL_UNSIGNED_BYTE,
                GL_FALSE,
                render_buffer->stride(),
                reinterpret_cast<const GLvoid*>(attribute.offset));
              break;
            }
          }
        }
        break;
      case frontend::resource_command::type::k_target:
        {
          const auto render_target{resource->as_target};
          if (render_target->is_swapchain()) {
            // Swap chain targets don't have an user-defined attachments.
            break;
          }

          state->use_draw_target(render_target, nullptr);

          if (render_target->has_depth_stencil()) {
            const auto depth_stencil{render_target->depth_stencil()};
            // combined depth stencil format
            const auto texture{reinterpret_cast<const detail_es3::texture2D*>(depth_stencil + 1)};
            pglFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->tex, 0);
          } else {
            if (render_target->has_depth()) {
              const auto depth{render_target->depth()};
              const auto texture{reinterpret_cast<const detail_es3::texture2D*>(depth + 1)};
              pglFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture->tex, 0);
            } else if (render_target->has_stencil()) {
              const auto stencil{render_target->stencil()};
              const auto texture{reinterpret_cast<const detail_es3::texture2D*>(stencil + 1)};
              pglFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->tex, 0);
            }
          }

          // color attachments
          const auto& attachments{render_target->attachments()};
          for (rx_size i{0}; i < attachments.size(); i++) {
            const auto& attachment{attachments[i]};
            const auto attachment_enum{static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i)};
            switch (attachment.kind) {
            case frontend::target::attachment::type::k_texture2D:
              pglFramebufferTexture2D(
                GL_DRAW_FRAMEBUFFER,
                attachment_enum,
                GL_TEXTURE_2D,
                reinterpret_cast<detail_es3::texture2D*>(attachment.as_texture2D.texture + 1)->tex,
                static_cast<GLint>(attachment.level));
              break;
            case frontend::target::attachment::type::k_textureCM:
              pglFramebufferTexture2D(
                GL_DRAW_FRAMEBUFFER,
                attachment_enum,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(attachment.as_textureCM.face),
                reinterpret_cast<detail_es3::textureCM*>(attachment.as_textureCM.texture + 1)->tex,
                static_cast<GLint>(attachment.level));
              break;
            }
          }
        }
        break;
      case frontend::resource_command::type::k_program:
        {
          const auto render_program{resource->as_program};
          const auto program{reinterpret_cast<detail_es3::program*>(render_program + 1)};

          const auto shaders{render_program->shaders()};

          vector<GLuint> shader_handles;
          shaders.each_fwd([&](const frontend::shader& _shader) {
            GLuint shader_handle{compile_shader(render_program->uniforms(), _shader)};
            if (shader_handle != 0) {
              pglAttachShader(program->handle, shader_handle);
              shader_handles.push_back(shader_handle);
            }
          });

          pglLinkProgram(program->handle);

          GLint status{0};
          pglGetProgramiv(program->handle, GL_LINK_STATUS, &status);
          if (status != GL_TRUE) {
            GLint log_size{0};
            pglGetProgramiv(program->handle, GL_INFO_LOG_LENGTH, &log_size);

            logger(log::level::k_error, "failed linking program");

            if (log_size) {
              vector<char> error_log{&memory::g_system_allocator, static_cast<rx_size>(log_size)};
              pglGetProgramInfoLog(program->handle, log_size, &log_size, error_log.data());
              logger(log::level::k_error, "\n%s", error_log.data());
            }
          }

          shader_handles.each_fwd([&](GLuint _shader) {
            pglDetachShader(program->handle, _shader);
            pglDeleteShader(_shader);
          });

          // fetch uniform locations
          render_program->uniforms().each_fwd([program](const frontend::uniform& _uniform) {
            if (_uniform.is_padding()) {
              // Padding uniforms have index -1.
              program->uniforms.push_back(-1);
            } else {
              program->uniforms.push_back(pglGetUniformLocation(program->handle, _uniform.name().data()));
            }
          });
        }
        break;
      case frontend::resource_command::type::k_texture1D:
        {
          const auto render_texture{resource->as_texture1D};
          const auto wrap{render_texture->wrap()};
          const auto wrap_s{convert_texture_wrap(wrap)};
          const auto format{render_texture->format()};
          const auto filter{convert_texture_filter(render_texture->filter())};
          const auto& data{render_texture->data()};

          const auto levels{static_cast<GLint>(render_texture->levels())};

          state->use_texture(render_texture);

          pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, filter.min);
          pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, filter.mag);
          pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, wrap_s);
          pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_BASE_LEVEL, 0);
          pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, levels - 1);
          if (filter.requires_border_color()) {
            const math::vec4i color{(render_texture->border() * 255.0f).cast<rx_s32>()};
            pglTexParameteriv(GL_TEXTURE_1D, GL_TEXTURE_BORDER_COLOR, color.data());
          }

          for (GLint i{0}; i < levels; i++) {
            const auto level_info{render_texture->info_for_level(i)};
            if (render_texture->is_compressed_format()) {
              pglCompressedTexImage1D(
                GL_TEXTURE_1D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions),
                0,
                level_info.size,
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            } else {
              pglTexImage1D(
                GL_TEXTURE_1D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions),
                0,
                convert_texture_format(format),
                convert_texture_data_type(format),
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            }
          }
        }
        break;
      case frontend::resource_command::type::k_texture2D:
        {
          const auto render_texture{resource->as_texture2D};
          if (render_texture->is_swapchain()) {
            break;
          }

          const auto wrap{render_texture->wrap()};
          const auto wrap_s{convert_texture_wrap(wrap.s)};
          const auto wrap_t{convert_texture_wrap(wrap.t)};
          const auto format{render_texture->format()};
          const auto filter{convert_texture_filter(render_texture->filter())};
          const auto& data{render_texture->data()};

          const auto levels{static_cast<GLint>(render_texture->levels())};

          state->use_texture(render_texture);

          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter.min);
          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter.mag);
          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
          pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
          if (filter.requires_border_color()) {
            const math::vec4i color{(render_texture->border() * 255.0f).cast<rx_s32>()};
            pglTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color.data());
          }

          for (GLint i{0}; i < levels; i++) {
            const auto level_info{render_texture->info_for_level(i)};
            if (render_texture->is_compressed_format()) {
              pglCompressedTexImage2D(
                GL_TEXTURE_2D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions.w),
                static_cast<GLsizei>(level_info.dimensions.h),
                0,
                level_info.size,
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            } else {
              pglTexImage2D(
                GL_TEXTURE_2D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions.w),
                static_cast<GLsizei>(level_info.dimensions.h),
                0,
                convert_texture_format(format),
                convert_texture_data_type(format),
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            }
          }
        }
        break;
      case frontend::resource_command::type::k_texture3D:
        {
          const auto render_texture{resource->as_texture3D};
          const auto wrap{render_texture->wrap()};
          const auto wrap_s{convert_texture_wrap(wrap.s)};
          const auto wrap_t{convert_texture_wrap(wrap.t)};
          const auto wrap_r{convert_texture_wrap(wrap.p)};
          const auto format{render_texture->format()};
          const auto filter{convert_texture_filter(render_texture->filter())};
          const auto& data{render_texture->data()};

          const auto levels{static_cast<GLint>(render_texture->levels())};

          state->use_texture(render_texture);

          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter.min);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, filter.mag);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap_s);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap_t);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap_r);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
          pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, levels - 1);
          if (filter.requires_border_color()) {
            const math::vec4i color{(render_texture->border() * 255.0f).cast<rx_s32>()};
            pglTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, color.data());
          }

          for (GLint i{0}; i < levels; i++) {
            const auto level_info{render_texture->info_for_level(i)};
            if (render_texture->is_compressed_format()) {
              pglCompressedTexImage3D(
                GL_TEXTURE_3D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions.w),
                static_cast<GLsizei>(level_info.dimensions.h),
                static_cast<GLsizei>(level_info.dimensions.d),
                0,
                level_info.size,
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            } else {
              pglTexImage3D(
                GL_TEXTURE_3D,
                i,
                convert_texture_data_format(format),
                static_cast<GLsizei>(level_info.dimensions.w),
                static_cast<GLsizei>(level_info.dimensions.h),
                static_cast<GLsizei>(level_info.dimensions.d),
                0,
                convert_texture_format(format),
                convert_texture_data_type(format),
                data.is_empty() ? nullptr : data.data() + level_info.offset);
            }
          }
        }
        break;
      case frontend::resource_command::type::k_textureCM:
        {
          const auto render_texture{resource->as_textureCM};
          const auto wrap{render_texture->wrap()};
          const auto wrap_s{convert_texture_wrap(wrap.s)};
          const auto wrap_t{convert_texture_wrap(wrap.t)};
          const auto wrap_p{convert_texture_wrap(wrap.p)};
          const auto format{render_texture->format()};
          const auto filter{convert_texture_filter(render_texture->filter())};
          const auto& data{render_texture->data()};

          const auto levels{static_cast<GLint>(render_texture->levels())};

          state->use_texture(render_texture);

          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filter.min);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filter.mag);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap_s);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap_t);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap_p);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
          pglTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, levels - 1);
          if (filter.requires_border_color()) {
            const math::vec4i color{(render_texture->border() * 255.0f).cast<rx_s32>()};
            pglTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, color.data());
          }

          for (GLint i{0}; i < levels; i++) {
            const auto level_info{render_texture->info_for_level(i)};
            for (GLint j{0}; j < 6; j++) {
              if (render_texture->is_compressed_format()) {
                pglCompressedTexImage2D(
                  GL_TEXTURE_CUBE_MAP_POSITIVE_X + j,
                  i,
                  convert_texture_data_format(format),
                  static_cast<GLsizei>(level_info.dimensions.w),
                  static_cast<GLsizei>(level_info.dimensions.h),
                  0,
                  level_info.size / 6,
                  data.is_empty() ? nullptr : data.data() + level_info.offset + level_info.size / 6 * j);
              } else {
                pglTexImage2D(
                  GL_TEXTURE_CUBE_MAP_POSITIVE_X + j,
                  i,
                  convert_texture_data_format(format),
                  static_cast<GLsizei>(level_info.dimensions.w),
                  static_cast<GLsizei>(level_info.dimensions.h),
                  0,
                  convert_texture_format(format),
                  convert_texture_data_type(format),
                  data.is_empty() ? nullptr : data.data() + level_info.offset + level_info.size / 6 * j);
              }
            }
          }
        }
        break;
      }
    }
    break;
  case frontend::command_type::k_resource_update:
    {
      const auto resource{reinterpret_cast<const frontend::resource_command*>(header + 1)};
      switch (resource->kind) {
      case frontend::resource_command::type::k_buffer:
        {
          const auto render_buffer{resource->as_buffer};
          auto buffer{reinterpret_cast<detail_es3::buffer*>(render_buffer + 1)};
          const auto& vertices{render_buffer->vertices()};
          const auto& elements{render_buffer->elements()};
          const auto type{render_buffer->kind() == frontend::buffer::type::k_dynamic
            ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW};

          state->use_buffer(render_buffer);

          state->use_vbo(buffer->bo[0]);
          state->use_ebo(buffer->bo[1]);

          if (vertices.size() > buffer->vertices_size) {
            // respecify buffer storage if we exceed what is available to use
            buffer->vertices_size = vertices.size();
            pglBufferData(GL_ARRAY_BUFFER, vertices.size(), vertices.data(), type);
          } else {
            pglBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size(), vertices.data());
          }

          if (elements.size() > buffer->elements_size) {
            // respecify buffer storage if we exceed what is available to use
            buffer->elements_size = elements.size();
            pglBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size(), elements.data(), type);
          } else {
            pglBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, elements.size(), elements.data());
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case frontend::command_type::k_clear:
    {
      profiler::cpu_sample sample{"clear"};

      const auto command{reinterpret_cast<frontend::clear_command*>(header + 1)};
      const auto render_state{reinterpret_cast<frontend::state*>(command)};
      const auto render_target{command->render_target};
      const bool clear_depth{command->clear_depth};
      const bool clear_stencil{command->clear_stencil};

      state->use_state(render_state);
      state->use_draw_target(render_target, &command->draw_buffers);

      if (command->clear_colors) {
        for (rx_u32 i{0}; i < sizeof command->color_values / sizeof *command->color_values; i++) {
          if (command->clear_colors & (1 << i)) {
            pglClearBufferfv(GL_COLOR, static_cast<GLint>(i),
              command->color_values[i].data());
          }
        }
      }

      if (clear_depth && clear_stencil) {
        pglClearBufferfi(GL_DEPTH_STENCIL, 0, command->depth_value,
          command->stencil_value);
      } else if (clear_depth) {
        pglClearBufferfv(GL_DEPTH, 0, &command->depth_value);
      } else if (clear_stencil) {
        const GLint value{command->stencil_value};
        pglClearBufferiv(GL_STENCIL, 0, &value);
      }
    }
    break;
  case frontend::command_type::k_draw:
    {
      profiler::cpu_sample sample{"draw"};

      const auto command{reinterpret_cast<frontend::draw_command*>(header + 1)};
      const auto render_state{reinterpret_cast<frontend::state*>(command)};
      const auto render_target{command->render_target};
      const auto render_buffer{command->render_buffer};
      const auto render_program{command->render_program};
      const auto this_program{reinterpret_cast<detail_es3::program*>(render_program + 1)};

      state->use_draw_target(render_target, &command->draw_buffers);
      state->use_buffer(render_buffer);
      state->use_program(render_program);
      state->use_state(render_state);

      // check for and apply uniform deltas
      if (command->dirty_uniforms_bitset) {
        const auto& program_uniforms{render_program->uniforms()};
        const rx_byte* draw_uniforms{command->uniforms()};

        for (rx_size i{0}; i < 64; i++) {
          if (command->dirty_uniforms_bitset & (1_u64 << i)) {
            const auto& uniform{program_uniforms[i]};
            const auto location{this_program->uniforms[i]};

            if (location == -1) {
              draw_uniforms += uniform.size();
              continue;
            }

            switch (uniform.kind()) {
            case frontend::uniform::type::k_sampler1D:
              [[fallthrough]];
            case frontend::uniform::type::k_sampler2D:
              [[fallthrough]];
            case frontend::uniform::type::k_sampler3D:
              [[fallthrough]];
            case frontend::uniform::type::k_samplerCM:
              pglUniform1i(location,
                *reinterpret_cast<const rx_s32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_bool:
              pglUniform1i(location,
                *reinterpret_cast<const bool*>(draw_uniforms) ? 1 : 0);
              break;
            case frontend::uniform::type::k_int:
              pglUniform1i(location,
                *reinterpret_cast<const rx_s32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_float:
              pglUniform1fv(location, 1,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec2i:
              pglUniform2iv(location, 1,
                reinterpret_cast<const rx_s32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec3i:
              pglUniform3iv(location, 1,
                reinterpret_cast<const rx_s32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec4i:
              pglUniform4iv(location, 1,
                reinterpret_cast<const rx_s32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec2f:
              pglUniform2fv(location, 1,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec3f:
              pglUniform3fv(location, 1,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_vec4f:
              pglUniform4fv(location, 1,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_mat3x3f:
              pglUniformMatrix3fv(location, 1, GL_FALSE,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_mat4x4f:
              pglUniformMatrix4fv(location, 1, GL_FALSE,
                reinterpret_cast<const rx_f32*>(draw_uniforms));
              break;
            case frontend::uniform::type::k_bonesf:
              pglUniformMatrix3x4fv(location,
                static_cast<GLsizei>(uniform.size() / sizeof(math::mat3x4f)),
                GL_FALSE, reinterpret_cast<const rx_f32*>(draw_uniforms));
            }

            draw_uniforms += uniform.size();
          }
        }
      }

      // apply any textures
      for (rx_size i{0}; i < frontend::draw_command::k_max_textures; i++) {
        const int ch{command->texture_types[i]};
        if (ch == '\0') {
          break;
        }
        switch (ch) {
        case '1':
          state->use_active_texture(reinterpret_cast<frontend::texture1D*>(command->texture_binds[i]), i);
          break;
        case '2':
          state->use_active_texture(reinterpret_cast<frontend::texture2D*>(command->texture_binds[i]), i);
          break;
        case '3':
          state->use_active_texture(reinterpret_cast<frontend::texture3D*>(command->texture_binds[i]), i);
          break;
        case 'c':
          state->use_active_texture(reinterpret_cast<frontend::textureCM*>(command->texture_binds[i]), i);
          break;
        default:
          break;
        }
      }

      switch (render_buffer->element_kind()) {
      case frontend::buffer::element_type::k_u8:
        pglDrawElements(
          convert_primitive_type(command->type),
          static_cast<GLsizei>(command->count),
          GL_UNSIGNED_BYTE,
          reinterpret_cast<void*>(sizeof(GLubyte) * command->offset));
        break;
      case frontend::buffer::element_type::k_u16:
        pglDrawElements(
          convert_primitive_type(command->type),
          static_cast<GLsizei>(command->count),
          GL_UNSIGNED_SHORT,
          reinterpret_cast<void*>(sizeof(GLushort) * command->offset));
        break;
      case frontend::buffer::element_type::k_u32:
        pglDrawElements(
          convert_primitive_type(command->type),
          static_cast<GLsizei>(command->count),
          GL_UNSIGNED_INT,
          reinterpret_cast<void*>(sizeof(GLuint) * command->offset));
        break;
      case frontend::buffer::element_type::k_none:
        pglDrawArrays(
          convert_primitive_type(command->type),
          static_cast<GLint>(command->offset),
          static_cast<GLsizei>(command->count));
        break;
      }
    }
    break;
  case frontend::command_type::k_blit:
    {
      profiler::cpu_sample sample{"blit"};

      const auto command{reinterpret_cast<frontend::blit_command*>(header + 1)};
      const auto render_state{reinterpret_cast<frontend::state*>(command)};

      // TODO(dweiler): optimize use_state to only consider the things that matter
      // during a blit operation:
      //
      // * scissor test
      // * blend write mask
      state->use_state(render_state);

      auto* src_render_target{command->src_target};
      auto* dst_render_target{command->dst_target};

      const auto src_attachment{command->src_attachment};
      const auto dst_attachment{command->dst_attachment};

      const auto src_dimensions{src_render_target->attachments()[src_attachment].as_texture2D.texture->dimensions().cast<GLint>()};
      const auto dst_dimensions{dst_render_target->attachments()[dst_attachment].as_texture2D.texture->dimensions().cast<GLint>()};

      frontend::buffers draw_buffers;
      frontend::buffers read_buffers;
      memset(&draw_buffers, 0, sizeof draw_buffers);
      memset(&read_buffers, 0, sizeof read_buffers);
      draw_buffers.add(dst_attachment);
      read_buffers.add(src_attachment);

      state->use_read_target(src_render_target, &read_buffers);
      state->use_draw_target(dst_render_target, &draw_buffers);

      pglBlitFramebuffer(
        0,
        0,
        src_dimensions.w,
        src_dimensions.h,
        0,
        0,
        dst_dimensions.w,
        dst_dimensions.h,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);

      break;
    }
  case frontend::command_type::k_profile:
    {
      const auto command{reinterpret_cast<frontend::profile_command*>(header + 1)};
      if (command->tag) {
        rmt_BeginOpenGLSampleDynamic(command->tag);
      } else {
        rmt_EndOpenGLSample();
      }
      break;
    }
  }
}

void es3::swap() {
  profiler::cpu_sample sample{"es3::swap"};
  SDL_GL_SwapWindow(reinterpret_cast<SDL_Window*>(m_data));
}

} // namespace rx::backend
