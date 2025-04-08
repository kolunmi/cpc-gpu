/* cpc-gpu-gl.c
 *
 * Copyright 2025 Adam Masciola
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "CpcGpuGL"
#include "cpc-gpu-private.h"

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#else
#define GLAD_MALLOC g_malloc
#define GLAD_FREE g_free
#define GLAD_GL_IMPLEMENTATION
#include "external/glad.h"
#endif

#define CGL_MESSAGE_PREFIX "OpenGL Backend: "
#define CGL_CRITICAL(...) g_critical (CGL_MESSAGE_PREFIX __VA_ARGS__)
#define CGL_CRITICAL_USER_ERROR(...) g_critical (CGL_MESSAGE_PREFIX "User Error: " __VA_ARGS__)

#define CGL_DESTROYED_OBJECTS_LOCK_BIT (CG_PRIV_DATA_LOCK_BIT - 1)
#define CGL_ENTER_DESTROYED_OBJECTS(gpu) CG_PRIV_ENTER_BIT (gpu, CGL_DESTROYED_OBJECTS_LOCK_BIT)
#define CGL_LEAVE_DESTROYED_OBJECTS(gpu) CG_PRIV_LEAVE_BIT (gpu, CGL_DESTROYED_OBJECTS_LOCK_BIT)

typedef struct
{
  char *name;
  int location;
  int num;
  GLenum type;
} ShaderLocation;

enum
{
  OBJECT_SHADER = 0,
  OBJECT_BUFFER,
  OBJECT_VERTEX_ARRAY,
  OBJECT_TEXTURE,
};

typedef struct
{
  GLenum type;
  GLuint id;
} DestroyedObject;

typedef struct _CglGpu CglGpu;
typedef struct _CglPlan CglPlan;
typedef struct _CglShader CglShader;
typedef struct _CglBuffer CglBuffer;
typedef struct _CglTexture CglTexture;
typedef struct _CglCommands CglCommands;

struct _CglGpu
{
  CgGpu base;
  gatomicrefcount refcount;

  int n_extensions;
  int max_texture_size;

  GArray *framebuffer_stack;
  GArray *destroyed_objects;
};

struct _CglPlan
{
  CgPlan base;
  gatomicrefcount refcount;
};

struct _CglShader
{
  CgShader base;
  gatomicrefcount refcount;

  GLuint program;

  ShaderLocation *attributes;
  guint n_attributes;
  GHashTable *attribute_assoc;

  ShaderLocation *uniforms;
  guint n_uniforms;
  GHashTable *uniform_assoc;
  GHashTable *uniform_blocks;
};

struct _CglBuffer
{
  CgBuffer base;
  gatomicrefcount refcount;

  GLuint vao_id;
  GLuint vbo_id;
  GLuint ubo_id;

  guint length;
  gboolean dynamic;
};

struct _CglTexture
{
  CgTexture base;
  gatomicrefcount refcount;

  GLuint id;

  CgTexture *non_msaa;
};

struct _CglCommands
{
  CgCommands base;
  gatomicrefcount refcount;

  GNode *instrs;
};

static void
_cgl_set_error (GError **error,
                int code,
                char *msg_to_free)
{
  g_autoptr (GString) builder = NULL;
  GLenum gl_error = 0;

  builder = g_string_new_take (msg_to_free);

  g_string_append (builder, "\nglGetError () BEGIN:\n");

  for (guint idx = 0; (gl_error = glGetError ()) != GL_NO_ERROR; idx++)
    {
      const char *error_string = "Error Not Recognized!";

      switch (gl_error)
        {
        case GL_INVALID_ENUM:
          error_string = "GL_INVALID_ENUM";
          break;
        case GL_INVALID_VALUE:
          error_string = "GL_INVALID_VALUE";
          break;
        case GL_INVALID_OPERATION:
          error_string = "GL_INVALID_OPERATION";
          break;
        case GL_STACK_OVERFLOW:
          error_string = "GL_STACK_OVERFLOW";
          break;
        case GL_STACK_UNDERFLOW:
          error_string = "GL_STACK_UNDERFLOW";
          break;
        case GL_OUT_OF_MEMORY:
          error_string = "GL_OUT_OF_MEMORY";
          break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
          error_string = "GL_INVALID_FRAMEBUFFER_OPERATION";
          break;
          // case GL_CONTEXT_LOST:
          //   error_string = "GL_CONTEXT_LOST";
          //   break;
        default:
          break;
        }

      g_string_append_printf (
          builder, "  %d: %s (0x%x)\n", idx, error_string, gl_error);
    }

  g_string_append (builder, "glGetError () END");

  g_set_error_literal (error, CG_ERROR, code, builder->str);
}

#define CGL_SET_ERROR(error, code, ...)                                \
  G_STMT_START                                                         \
  {                                                                    \
    if (error != NULL)                                                 \
      _cgl_set_error ((error), (code), g_strdup_printf (__VA_ARGS__)); \
  }                                                                    \
  G_STMT_END

static GPrivate current_context = G_PRIVATE_INIT (cg_gpu_unref);

static CgGpu *
get_gpu_for_this_thread (void)
{
  return g_private_get (&current_context);
}

static void
set_gpu_for_this_thread (CgGpu *gpu)
{
  g_private_replace (&current_context,
                     gpu != NULL ? cg_gpu_ref (gpu) : NULL);
}

static void GLAPIENTRY
debug_cb (GLenum source,
          GLenum type,
          GLuint id,
          GLenum severity,
          GLsizei length,
          const GLchar *message,
          const void *userParam)
{
  const char *source_string = NULL;
  const char *type_string = NULL;
  const char *severity_string = NULL;

  switch (source)
    {
    case GL_DEBUG_SOURCE_API:
      source_string = "GL_DEBUG_SOURCE_API";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      source_string = "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      source_string = "GL_DEBUG_SOURCE_SHADER_COMPILER";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      source_string = "GL_DEBUG_SOURCE_THIRD_PARTY";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      source_string = "GL_DEBUG_SOURCE_APPLICATION";
      break;
    case GL_DEBUG_SOURCE_OTHER:
      source_string = "GL_DEBUG_SOURCE_OTHER";
      break;
    default:
      break;
    }

  switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
      type_string = "GL_DEBUG_TYPE_ERROR";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      type_string = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      type_string = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      type_string = "GL_DEBUG_TYPE_PORTABILITY";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      type_string = "GL_DEBUG_TYPE_PORTABILITY";
      break;
    case GL_DEBUG_TYPE_MARKER:
      type_string = "GL_DEBUG_TYPE_MARKER";
      break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
      type_string = "GL_DEBUG_TYPE_PUSH_GROUP";
      break;
    case GL_DEBUG_TYPE_POP_GROUP:
      type_string = "GL_DEBUG_TYPE_POP_GROUP";
      break;
    case GL_DEBUG_TYPE_OTHER:
      type_string = "GL_DEBUG_TYPE_OTHER";
      break;
    default:
      break;
    }

  switch (severity)
    {
    case GL_DEBUG_SEVERITY_LOW:
      severity_string = "GL_DEBUG_SEVERITY_LOW";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      severity_string = "GL_DEBUG_SEVERITY_MEDIUM";
      break;
    case GL_DEBUG_SEVERITY_HIGH:
      severity_string = "GL_DEBUG_SEVERITY_HIGH";
      break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      severity_string = "GL_DEBUG_SEVERITY_NOTIFICATION";
      break;
    default:
      break;
    }

  g_debug ("GL: DIRECT GL MESSAGE (%s, %s, %s): %s",
           source_string, type_string, severity_string, message);
}

static void
clear_destroyed_object (DestroyedObject *self)
{
  switch (self->type)
    {
    case OBJECT_SHADER:
      glDeleteProgram (self->id);
      break;
    case OBJECT_BUFFER:
      glDeleteBuffers (1, &self->id);
      break;
    case OBJECT_VERTEX_ARRAY:
      glDeleteVertexArrays (1, &self->id);
      break;
    case OBJECT_TEXTURE:
      glDeleteTextures (1, &self->id);
      break;
    default:
      g_assert_not_reached ();
    }
}

static CgGpu *
gpu_new (guint32 flags,
         gpointer extra_data,
         GError **error)
{
#ifndef USE_EPOXY
  GLADloadfunc extensions_loader = extra_data;
#endif
  g_autoptr (CgGpu) gpu = NULL;
  CglGpu *gl_gpu = NULL;

  gpu = (CgGpu *)CG_PRIV_CREATE (gl_gpu);
  gpu->impl = &cg_gl_impl;
  gl_gpu = (CglGpu *)gpu;
  g_atomic_ref_count_init (&gl_gpu->refcount);

#ifndef USE_EPOXY
  if (extensions_loader != NULL
      && gladLoadGL (extensions_loader) == 0)
    {
      CGL_SET_ERROR (error, CG_ERROR_FAILED_INIT,
                     "Failed to load OpenGL extensions");
      return NULL;
    }
#endif

  glGetIntegerv (GL_NUM_EXTENSIONS, &gl_gpu->n_extensions);
  g_debug ("GL: Loaded %d GL extensions", gl_gpu->n_extensions);
  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_gpu->max_texture_size);
  g_debug ("GL: The max texture size is %d", gl_gpu->max_texture_size);

  if (flags & CG_INIT_FLAG_USE_DEBUG_LAYERS)
    {
      glDebugMessageCallback (debug_cb, 0);
      glEnable (GL_DEBUG_OUTPUT);
      glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
      g_debug ("GL: Enabled debug output");
    }

  glDepthFunc (GL_LEQUAL);
  glEnable (GL_DEPTH_TEST);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable (GL_BLEND);
  glCullFace (GL_BACK);
  glFrontFace (GL_CCW);
  glEnable (GL_CULL_FACE);
  glEnable (GL_MULTISAMPLE);

  gl_gpu->framebuffer_stack = g_array_new (FALSE, TRUE, sizeof (GLuint));
  gl_gpu->destroyed_objects = g_array_new (FALSE, TRUE, sizeof (DestroyedObject));
  g_array_set_clear_func (gl_gpu->destroyed_objects, (GDestroyNotify)clear_destroyed_object);

  return g_steal_pointer (&gpu);
}

static CgGpu *
gpu_ref (CgGpu *self)
{
  CglGpu *gl_gpu = (CglGpu *)self;

  g_atomic_ref_count_inc (&gl_gpu->refcount);
  return self;
}

static void
clear_gpu (CgGpu *self)
{
  CglGpu *gl_gpu = (CglGpu *)self;

  /* TODO could be the wrong thread? */
  glDeleteFramebuffers (gl_gpu->framebuffer_stack->len,
                        (GLuint *)gl_gpu->framebuffer_stack->data);
  g_clear_pointer (&gl_gpu->framebuffer_stack, g_array_unref);
  g_clear_pointer (&gl_gpu->destroyed_objects, g_array_unref);
}

static void
gpu_unref (CgGpu *self)
{
  CglGpu *gl_gpu = (CglGpu *)self;

  if (g_atomic_ref_count_dec (&gl_gpu->refcount))
    {
      clear_gpu (self);
      g_free (self);
    }
}

static const struct
{
  const char *k;
  GLenum v;
} gl_params[] = {
  { .k = "vendor", .v = GL_VENDOR },
  { .k = "renderer", .v = GL_RENDERER },
  { .k = "version", .v = GL_VERSION },
  { .k = "shading language version", .v = GL_SHADING_LANGUAGE_VERSION },
};

static char *
gpu_get_info (CgGpu *self,
              const char *param,
              GError **error)
{
  for (guint i = 0; i < G_N_ELEMENTS (gl_params); i++)
    if (g_str_equal (param, gl_params[i].k))
      return g_strdup ((const char *)glGetString (gl_params[i].v));

  return NULL;
}

static gboolean
gpu_flush (CgGpu *self,
           GError **error)
{
  CglGpu *gl_gpu = (CglGpu *)self;

  /* Registered clear func will release the resources. */
  g_array_set_size (gl_gpu->destroyed_objects, 0);

  return TRUE;
}

#define DESTROY_GL_OBJECT_ON_FLUSH(gpu, o_id, o_type)                    \
  G_STMT_START                                                           \
  {                                                                      \
    if ((o_id) > 0)                                                      \
      {                                                                  \
        DestroyedObject object = { 0 };                                  \
        object.id = (o_id);                                              \
        object.type = (o_type);                                          \
        CGL_ENTER_DESTROYED_OBJECTS (gpu);                               \
        g_array_append_val (((CglGpu *)gpu)->destroyed_objects, object); \
        CGL_LEAVE_DESTROYED_OBJECTS (gpu);                               \
      }                                                                  \
  }                                                                      \
  G_STMT_END

static void
init_plan (CgPlan *self)
{
}

static void
clear_plan (CgPlan *self)
{
  cg_priv_plan_finish (self);
}

static void
init_shader (CgShader *self)
{
  CglShader *gl_shader = (CglShader *)self;

  gl_shader->uniform_assoc = g_hash_table_new (g_str_hash, g_str_equal);
  gl_shader->uniform_blocks = g_hash_table_new (g_direct_hash, g_direct_equal);
  gl_shader->attribute_assoc = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
clear_shader (CgShader *self)
{
  CglShader *gl_shader = (CglShader *)self;

  DESTROY_GL_OBJECT_ON_FLUSH (self->gpu, gl_shader->program, OBJECT_SHADER);

  if (gl_shader->uniforms != NULL)
    for (guint i = 0; i < gl_shader->n_uniforms; i++)
      g_clear_pointer (&gl_shader->uniforms[i].name, g_free);
  g_clear_pointer (&gl_shader->uniforms, g_free);

  if (gl_shader->attributes != NULL)
    for (guint i = 0; i < gl_shader->n_attributes; i++)
      g_clear_pointer (&gl_shader->attributes[i].name, g_free);
  g_clear_pointer (&gl_shader->attributes, g_free);

  g_clear_pointer (&gl_shader->uniform_assoc, g_hash_table_unref);
  g_clear_pointer (&gl_shader->uniform_blocks, g_hash_table_unref);
  g_clear_pointer (&gl_shader->attribute_assoc, g_hash_table_unref);

  cg_priv_shader_finish (self);
}

static void
init_buffer (CgBuffer *self)
{
}

static void
clear_buffer (CgBuffer *self)
{
  CglBuffer *gl_buffer = (CglBuffer *)self;

  DESTROY_GL_OBJECT_ON_FLUSH (self->gpu, gl_buffer->vbo_id, OBJECT_BUFFER);
  DESTROY_GL_OBJECT_ON_FLUSH (self->gpu, gl_buffer->ubo_id, OBJECT_BUFFER);
  DESTROY_GL_OBJECT_ON_FLUSH (self->gpu, gl_buffer->vao_id, OBJECT_VERTEX_ARRAY);

  cg_priv_buffer_finish (self);
}

static void
init_texture (CgTexture *self)
{
}

static void
clear_texture (CgTexture *self)
{
  CglTexture *gl_texture = (CglTexture *)self;

  DESTROY_GL_OBJECT_ON_FLUSH (self->gpu, gl_texture->id, OBJECT_TEXTURE);
  g_clear_pointer (&gl_texture->non_msaa, cg_texture_unref);

  cg_priv_texture_finish (self);
}

#undef DESTROY_GL_OBJECT_ON_FLUSH

static void
init_commands (CgCommands *self)
{
}

static void
clear_commands (CgCommands *self)
{
  CglCommands *gl_commands = (CglCommands *)self;

  g_clear_pointer (&gl_commands->instrs, cg_priv_destroy_instr_node);
  cg_priv_commands_finish (self);
}

#define DEFINE_BASIC_OBJECT(name, type, parent_type)        \
  static parent_type *                                      \
      name##_new (CgGpu *self)                              \
  {                                                         \
    parent_type *obj = (parent_type *)g_new0 (type, 1);     \
    g_atomic_ref_count_init (&((type *)obj)->refcount);     \
    init_##name (obj);                                      \
    return obj;                                             \
  }                                                         \
  static parent_type *                                      \
      name##_ref (parent_type *self)                        \
  {                                                         \
    g_atomic_ref_count_inc (&((type *)self)->refcount);     \
    return self;                                            \
  }                                                         \
  static void                                               \
      name##_unref (parent_type *self)                      \
  {                                                         \
    if (g_atomic_ref_count_dec (&((type *)self)->refcount)) \
      {                                                     \
        clear_##name (self);                                \
        g_free (self);                                      \
      }                                                     \
  }

DEFINE_BASIC_OBJECT (plan, CglPlan, CgPlan)
DEFINE_BASIC_OBJECT (shader, CglShader, CgShader)
DEFINE_BASIC_OBJECT (buffer, CglBuffer, CgBuffer)
DEFINE_BASIC_OBJECT (texture, CglTexture, CgTexture)
DEFINE_BASIC_OBJECT (commands, CglCommands, CgCommands)

#undef DEFINE_BASIC_OBJECT

static guint
compile_shader (const char *code,
                int type,
                GError **error)
{
  guint shader = 0;
  GLint success = 0;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &code, NULL);
  glCompileShader (shader);
  glGetShaderiv (shader, GL_COMPILE_STATUS, &success);

  if (success != GL_TRUE)
    {
      const char *type_string = "generic";
      GLint max_error_size = 0;
      g_autofree char *error_string = NULL;

      switch (type)
        {
        case GL_VERTEX_SHADER:
          type_string = "vertex";
          break;
        case GL_FRAGMENT_SHADER:
          type_string = "fragment";
          break;
        case GL_COMPUTE_SHADER:
          type_string = "compute";
          break;
        default:
          break;
        }

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &max_error_size);
      if (max_error_size > 0)
        {
          GLint size = 0;

          error_string = g_malloc (max_error_size);
          glGetShaderInfoLog (shader, max_error_size, &size, error_string);
        }

      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_SHADER_GEN,
          "Failed to generate %s shader: GL: %s",
          type_string, error_string);

      return 0;
    }

  return shader;
}

static gboolean
ensure_shader (CgShader *self,
               GError **error)
{
  CglShader *gl_shader = (CglShader *)self;
  guint vertex_id = 0;
  guint fragment_id = 0;
  guint program = 0;
  GLint link_success = 0;
  GLint n_attributes = 0;
  GLint n_uniform_blocks = 0;
  GLint n_uniforms = 0;
  g_autoptr (GArray) uniforms = NULL;

  if (gl_shader->program > 0) return TRUE;

  vertex_id = compile_shader (self->init.vertex_code, GL_VERTEX_SHADER, error);
  if (vertex_id == 0)
    return FALSE;

  fragment_id = compile_shader (self->init.fragment_code, GL_FRAGMENT_SHADER, error);
  if (fragment_id == 0)
    {
      glDeleteShader (vertex_id);
      return FALSE;
    }

  program = glCreateProgram ();
  glAttachShader (program, vertex_id);
  glAttachShader (program, fragment_id);
  glLinkProgram (program);

  glGetProgramiv (program, GL_LINK_STATUS, &link_success);
  if (link_success != GL_TRUE)
    {
      GLint max_error_size = 0;
      g_autofree char *error_string = NULL;

      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &max_error_size);

      if (max_error_size > 0)
        {
          GLint size = 0;

          error_string = g_malloc (max_error_size);

          glGetProgramInfoLog (program, max_error_size, &size, error_string);

          error_string[size] = '\0';
        }

      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_SHADER_GEN,
          "Failed to link shader: GL: %s",
          error_string);

      glDeleteProgram (program);
      return FALSE;
    }

  gl_shader->program = program;

  /* --- Attributes --- */
  glGetProgramiv (program, GL_ACTIVE_ATTRIBUTES, &n_attributes);
  gl_shader->attributes = g_malloc0_n (n_attributes, sizeof (ShaderLocation));
  gl_shader->n_attributes = n_attributes;

  for (int i = 0; i < n_attributes; i++)
    {
      GLint namelen = 0;
      GLint num = 0;
      char name[256] = { 0 };
      GLenum type = GL_ZERO;

      glGetActiveAttrib (program, i, sizeof (name) - 1,
                         &namelen, &num, &type, name);
      name[namelen] = '\0';

      /* TODO make this more memory-efficient */
      gl_shader->attributes[i].name = g_strdup (name);
      gl_shader->attributes[i].location = i;
      gl_shader->attributes[i].num = num;
      gl_shader->attributes[i].type = type;

      g_hash_table_replace (
          gl_shader->attribute_assoc,
          gl_shader->attributes[i].name,
          gl_shader->attributes + i);
    }

  /* --- Uniforms --- */
  glGetProgramiv (program, GL_ACTIVE_UNIFORMS, &n_uniforms);
  uniforms = g_array_new (FALSE, TRUE, sizeof (ShaderLocation));

  for (int i = 0, location = 0; i < n_uniforms; i++)
    {
      GLint namelen = 0;
      GLint num = 0;
      char name[256] = { 0 };
      GLenum type = GL_ZERO;
      ShaderLocation uniform = { 0 };

      glGetActiveUniform (program, i, sizeof (name) - 1,
                          &namelen, &num, &type, name);
      name[namelen] = '\0';

      if (num > 1)
        {
          char *bracket = NULL;

          bracket = strchr (name, '[');
          if (bracket != NULL) *bracket = '\0';
        }

      uniform.name = g_strdup (name);
      uniform.location = location;
      uniform.num = num;
      uniform.type = type;
      g_array_append_val (uniforms, uniform);

      /* Map the uniform name to its corresponding location + 1. */
      g_hash_table_replace (
          gl_shader->uniform_assoc,
          uniform.name,
          GUINT_TO_POINTER (uniforms->len));

      location += num;
    }

  gl_shader->n_uniforms = uniforms->len;
  gl_shader->uniforms = (ShaderLocation *)g_array_free (g_steal_pointer (&uniforms), FALSE);

  /* --- Uniform Blocks --- */
  glGetProgramiv (program, GL_ACTIVE_UNIFORM_BLOCKS, &n_uniform_blocks);

  for (int i = 0; i < n_uniform_blocks; i++)
    {
      GLint n_block_uniforms = 0;
      g_autofree GLint *block_uniforms = NULL;

      glGetActiveUniformBlockiv (program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &n_block_uniforms);
      if (n_block_uniforms == 0) continue;

      block_uniforms = g_malloc0_n (n_block_uniforms, sizeof (*block_uniforms));
      glGetActiveUniformBlockiv (program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, block_uniforms);

      /* Map the uniform location to its corresponding block + 1. */
      for (int j = 0; j < n_block_uniforms; j++)
        g_hash_table_replace (
            gl_shader->uniform_blocks,
            GINT_TO_POINTER (gl_shader->uniforms[block_uniforms[j]].location),
            GINT_TO_POINTER (i + 1));
    }

  return TRUE;
}

static gboolean
ensure_buffer (CgBuffer *self,
               GError **error)
{
  CglBuffer *gl_buffer = (CglBuffer *)self;
  guint ubo_id = 0;

  if (gl_buffer->vao_id > 0)
    {
      CGL_CRITICAL_USER_ERROR (
          "Buffer previously initialized as a vertex buffer "
          "erroneously being used as a uniform buffer");
      return FALSE;
    }
  if (gl_buffer->vbo_id > 0)
    return TRUE;

  glGenBuffers (1, &ubo_id);
  if (ubo_id == 0)
    {
      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_BUFFER_GEN,
          "Failed to generate uniform buffer object");
      return FALSE;
    }

  glBindBuffer (GL_UNIFORM_BUFFER, ubo_id);
  glBufferData (GL_UNIFORM_BUFFER, self->init.size,
                self->init.data, GL_STATIC_DRAW);
  glBindBuffer (GL_UNIFORM_BUFFER, 0);

  gl_buffer->vao_id = 0;
  gl_buffer->vbo_id = 0;
  gl_buffer->ubo_id = ubo_id;
  gl_buffer->length = 0;
  gl_buffer->dynamic = TRUE;

  return TRUE;
}

static gboolean
ensure_vertices (CgBuffer *self,
                 GError **error)
{
  CglBuffer *gl_buffer = (CglBuffer *)self;
  guint vao_id = 0;
  guint vbo_id = 0;

  if (gl_buffer->ubo_id > 0)
    {
      CGL_CRITICAL_USER_ERROR (
          "Buffer previously initialized as a uniform buffer "
          "erroneously being used as a vertex buffer");
      return FALSE;
    }
  if (gl_buffer->vao_id > 0 && gl_buffer->vbo_id > 0)
    return TRUE;
  g_assert (gl_buffer->vao_id == 0 && gl_buffer->vbo_id == 0);

  if (self->spec == NULL)
    {
      CGL_CRITICAL_USER_ERROR (
          "Buffer needs a layout specification "
          "to be used as an attribute");
      return FALSE;
    }

  glGenVertexArrays (1, &vao_id);
  if (vao_id == 0)
    {
      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_BUFFER_GEN,
          "Failed to generate vertex array object");
      return FALSE;
    }

  glGenBuffers (1, &vbo_id);
  if (vbo_id == 0)
    {
      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_BUFFER_GEN,
          "Failed to generate vertex buffer object");
      glDeleteVertexArrays (1, &vao_id);
      return FALSE;
    }

  /* TODO: Check for errors here */
  glBindBuffer (GL_ARRAY_BUFFER, vbo_id);
  glBufferData (GL_ARRAY_BUFFER, self->init.size,
                self->init.data, GL_STATIC_DRAW);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  gl_buffer->vao_id = vao_id;
  gl_buffer->vbo_id = vbo_id;
  gl_buffer->ubo_id = 0;
  gl_buffer->length = self->init.size;
  gl_buffer->dynamic = TRUE;

  return TRUE;
}

static inline gsize
get_image_size (int width,
                int height,
                int format)
{
  gsize bytes_per_pixel = 0;

  switch (format)
    {
    case CG_FORMAT_R8:
      bytes_per_pixel = 1;
      break;
    case CG_FORMAT_RA8:
      bytes_per_pixel = 2;
      break;
    case CG_FORMAT_RGB8:
      bytes_per_pixel = 3;
      break;
    case CG_FORMAT_RGBA8:
      bytes_per_pixel = 4;
      break;
    case CG_FORMAT_R32:
      bytes_per_pixel = 4;
      break;
    case CG_FORMAT_RGB32:
      bytes_per_pixel = 12;
      break;
    case CG_FORMAT_RGBA32:
      bytes_per_pixel = 16;
      break;
    default:
      g_assert_not_reached ();
    }

  return bytes_per_pixel * width * height;
}

static gboolean
ensure_texture (CgTexture *self,
                GError **error)
{
  CglTexture *gl_texture = (CglTexture *)self;
  GLuint gl_internal = 0;
  GLuint gl_format = 0;
  GLuint gl_type = 0;
  gsize image_size = 0;

  if (gl_texture->id > 0)
    return TRUE;

  glGenTextures (1, &gl_texture->id);
  if (gl_texture->id == 0)
    {
      CGL_SET_ERROR (
          error, CG_ERROR_FAILED_TEXTURE_GEN,
          "Failed to generate texture");
      return FALSE;
    }

  if (self->init.format == CG_PRIV_FORMAT_DEPTH)
    {
      glBindTexture (
          self->init.msaa > 0
              ? GL_TEXTURE_2D_MULTISAMPLE
              : GL_TEXTURE_2D,
          gl_texture->id);

      if (self->init.msaa > 0)
        glTexImage2DMultisample (
            GL_TEXTURE_2D_MULTISAMPLE, self->init.msaa,
            GL_DEPTH_COMPONENT, self->init.width,
            self->init.height, GL_TRUE);
      else
        glTexImage2D (
            GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
            self->init.width, self->init.height,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

      glBindTexture (
          self->init.msaa > 0
              ? GL_TEXTURE_2D_MULTISAMPLE
              : GL_TEXTURE_2D,
          0);

      return TRUE;
    }

  switch (self->init.format)
    {
    case CG_FORMAT_R8:
      gl_internal = GL_R8;
      gl_format = GL_RED;
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case CG_FORMAT_RA8:
      gl_internal = GL_RG8;
      gl_format = GL_RG;
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case CG_FORMAT_RGB8:
      gl_internal = GL_RGB8;
      gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case CG_FORMAT_RGBA8:
      gl_internal = GL_RGBA8;
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case CG_FORMAT_R32:
      gl_internal = GL_R32F;
      gl_format = GL_RED;
      gl_type = GL_FLOAT;
      break;
    case CG_FORMAT_RGB32:
      gl_internal = GL_RGB32F;
      gl_format = GL_RGB;
      gl_type = GL_FLOAT;
      break;
    case CG_FORMAT_RGBA32:
      gl_internal = GL_RGBA32F;
      gl_format = GL_RGBA;
      gl_type = GL_FLOAT;
      break;
    default:
      g_assert_not_reached ();
    }

  image_size = get_image_size (
      self->init.width, self->init.height, self->init.format);

  if (self->init.cubemap)
    {
      g_assert (self->init.data != NULL);

      glBindTexture (GL_TEXTURE_CUBE_MAP, gl_texture->id);

      for (int i = 0; i < 6; i++)
        {
          glTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                        0, gl_internal, self->init.width,
                        self->init.height, 0, gl_format, gl_type,
                        (guchar *)self->init.data + i * image_size);
        }

      glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

      glBindTexture (GL_TEXTURE_CUBE_MAP, 0);
    }
  else
    {
      if (self->init.msaa > 0)
        {
          glBindTexture (GL_TEXTURE_2D_MULTISAMPLE, gl_texture->id);

          glTexImage2DMultisample (
              GL_TEXTURE_2D_MULTISAMPLE, self->init.msaa,
              gl_internal, self->init.width, self->init.height, GL_TRUE);
        }
      else
        {
          int mip_width = self->init.width;
          int mip_height = self->init.height;

          glBindTexture (GL_TEXTURE_2D, gl_texture->id);

          for (int i = 0; i < self->init.mipmaps; i++)
            {
              glTexImage2D (
                  GL_TEXTURE_2D, i, gl_internal,
                  mip_width, mip_height, 0,
                  gl_format, gl_type, self->init.data);

              if (self->init.format == CG_FORMAT_R8
                  || self->init.format == CG_FORMAT_RA8)
                {
                  const GLint swizzle_mask[] = { GL_RED, GL_RED, GL_RED, GL_ALPHA };
                  glTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
                }

              mip_width /= 2;
              mip_height /= 2;

              if (mip_width < 1) mip_width = 1;
              if (mip_height < 1) mip_height = 1;
            }

          if (self->init.mipmaps > 1)
            {
              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }
        }

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      if (self->init.msaa > 0)
        glBindTexture (GL_TEXTURE_2D_MULTISAMPLE, 0);
      else
        glBindTexture (GL_TEXTURE_2D, 0);
    }

  return TRUE;
}

typedef struct
{
  gboolean failure;
  GError **error;
} EnsureData;

typedef struct
{
  EnsureData *ensure_data;
  GNode *start_node;
} ValidateUniformData;

static const GLenum type_to_uniform_map[][3] = {
  [CG_TYPE_SHADER] = { 0 },
  [CG_TYPE_BUFFER] = { 0 },
  [CG_TYPE_TEXTURE] = { GL_SAMPLER_2D, GL_SAMPLER_CUBE, 0 },
  [CG_TYPE_BOOL] = { GL_BOOL, 0 },
  [CG_TYPE_INT] = { GL_INT, 0 },
  [CG_TYPE_UINT] = { GL_UNSIGNED_INT, 0 },
  [CG_TYPE_FLOAT] = { GL_FLOAT, 0 },
  [CG_TYPE_POINTER] = { 0 },
  [CG_TYPE_VEC2] = { GL_FLOAT_VEC2, 0 },
  [CG_TYPE_VEC3] = { GL_FLOAT_VEC3, 0 },
  [CG_TYPE_VEC4] = { GL_FLOAT_VEC4, 0 },
  [CG_TYPE_MAT4] = { GL_FLOAT_MAT4, 0 },
  [CG_TYPE_RECT] = { 0 },
  [CG_TYPE_KEYVAL] = { 0 },
};

static gboolean
test_uniform_validity (
    const char *name,
    const CgValue *value,
    ValidateUniformData *data)
{
  GNode *node = data->start_node;

  do
    {
      CgPrivInstr *instr = node->data;

      if (instr->pass.shader != NULL)
        {
          guint index = 0;
          CglShader *gl_shader = NULL;
          ShaderLocation *location = NULL;
          gboolean match = FALSE;

          gl_shader = (CglShader *)instr->pass.shader;

          index = GPOINTER_TO_UINT (g_hash_table_lookup (
              gl_shader->uniform_assoc, name));

          if (index == 0)
            {
              CGL_SET_ERROR (
                  data->ensure_data->error, CG_ERROR_FAILED_SHADER_UNIFORM_SET,
                  "Uniform \"%s\" does not exist in shader",
                  name);
              return TRUE;
            }

          location = &gl_shader->uniforms[index - 1];

          for (guint i = 0; i < G_N_ELEMENTS (type_to_uniform_map[value->type]); i++)
            {
              if (type_to_uniform_map[value->type][i] == location->type)
                {
                  match = TRUE;
                  break;
                }
            }

          if (match)
            {
              if (value->type == CG_TYPE_TEXTURE)
                {
                  if (!ensure_texture (value->texture, data->ensure_data->error))
                    return TRUE;

                  if (value->texture->init.msaa > 0)
                    {
                      CglTexture *gl_texture = (CglTexture *)value->texture;

                      /* We must create a temporary texture to use as a uniform
                       * since msaa textures cannot be used in this way.
                       */
                      if (gl_texture->non_msaa == NULL)
                        {
                          gl_texture->non_msaa = texture_new (value->texture->gpu);
                          gl_texture->non_msaa->gpu = cg_gpu_ref (value->texture->gpu);
                          gl_texture->non_msaa->init.msaa = 0;
                          gl_texture->non_msaa->init.cubemap = value->texture->init.cubemap;
                          gl_texture->non_msaa->init.data = NULL;
                          gl_texture->non_msaa->init.width = value->texture->init.width;
                          gl_texture->non_msaa->init.height = value->texture->init.height;
                          gl_texture->non_msaa->init.format = value->texture->init.format;
                          gl_texture->non_msaa->init.mipmaps = value->texture->init.mipmaps;

                          if (!ensure_texture (gl_texture->non_msaa, data->ensure_data->error))
                            return TRUE;
                        }
                    }
                }
              else if (value->type == CG_TYPE_BUFFER
                       && !ensure_buffer (value->buffer, data->ensure_data->error))
                return TRUE;

              return FALSE;
            }
          else
            {
              int correct_type = CG_TYPE_0;

              for (int i = CG_TYPE_0 + 1; i < CG_N_TYPES; i++)
                {
                  gboolean breakout = FALSE;

                  for (int j = 0; j < G_N_ELEMENTS (type_to_uniform_map[i]); j++)
                    {
                      if (location->type == type_to_uniform_map[i][j])
                        {
                          correct_type = i;
                          breakout = TRUE;
                          break;
                        }
                    }

                  if (breakout) break;
                }

              if (correct_type == CG_TYPE_0)
                CGL_SET_ERROR (
                    data->ensure_data->error, CG_ERROR_FAILED_SHADER_UNIFORM_SET,
                    "The type of uniform \"%s\" is not currently supported.",
                    name);
              else
                CGL_SET_ERROR (
                    data->ensure_data->error, CG_ERROR_FAILED_SHADER_UNIFORM_SET,
                    "Submitted value type does not match shader type for uniform "
                    "\"%s\": expected %s, got %s",
                    name,
                    cg_priv_get_type_name (correct_type),
                    cg_priv_get_type_name (value->type));

              return TRUE;
            }
        }
    }
  while ((node = node->parent) != NULL);

  /* Frontend API should have verified that a shader was present. */
  g_assert_not_reached ();
}

typedef struct
{
  EnsureData *ensure_data;
  GNode *start_node;
} ValidateAttributesData;

static gboolean
test_attribute_validity (
    const char *name,
    gconstpointer value,
    ValidateAttributesData *data)
{
  GNode *node = data->start_node;

  do
    {
      CgPrivInstr *instr = node->data;

      if (instr->pass.shader != NULL)
        {
          CglShader *gl_shader = NULL;
          ShaderLocation *attribute = NULL;

          gl_shader = (CglShader *)instr->pass.shader;

          attribute = g_hash_table_lookup (
              gl_shader->attribute_assoc, name);

          if (attribute != NULL)
            return FALSE;
          else
            {
              CGL_SET_ERROR (
                  data->ensure_data->error, CG_ERROR_FAILED_SHADER_UNIFORM_SET,
                  "Attribute \"%s\" does not exist in shader",
                  name);
              return TRUE;
            }
        }
    }
  while ((node = node->parent) != NULL);

  g_assert_not_reached ();
}

static gboolean
ensure_instr_node (GNode *node,
                   EnsureData *data)
{
  CgPrivInstr *instr = node->data;

  switch (instr->type)
    {
    case CG_PRIV_INSTR_PASS:
      {
        ValidateUniformData uniform_data = { 0 };
        gconstpointer uniform_find_result = NULL;
        ValidateAttributesData attributes_data = { 0 };
        gconstpointer attribute_find_result = NULL;

        if (instr->pass.shader != NULL)
          {
            gboolean success = FALSE;

            success = ensure_shader (instr->pass.shader, data->error);
            if (!success)
              {
                data->failure = TRUE;
                return TRUE;
              }
          }

        for (guint i = 0; i < instr->pass.targets->len; i++)
          {
            gboolean success = FALSE;
            CgTexture *target = NULL;

            target = g_ptr_array_index (instr->pass.targets, i);
            success = ensure_texture (target, data->error);
            if (!success)
              {
                data->failure = TRUE;
                return TRUE;
              }
          }

        uniform_data.ensure_data = data;
        uniform_data.start_node = node;
        uniform_find_result = g_hash_table_find (
            instr->pass.uniforms.hash, (GHRFunc)test_uniform_validity, &uniform_data);
        if (uniform_find_result != NULL)
          {
            data->failure = TRUE;
            return TRUE;
          }

        attributes_data.ensure_data = data;
        attributes_data.start_node = node;
        attribute_find_result = g_hash_table_find (
            instr->pass.attributes, (GHRFunc)test_attribute_validity, &attributes_data);
        if (attribute_find_result != NULL)
          {
            data->failure = TRUE;
            return TRUE;
          }
      }
      break;
    case CG_PRIV_INSTR_VERTICES:
      if (instr->vertices.n_buffers > 1)
        {
          for (guint i = 0; i < instr->vertices.n_buffers; i++)
            {
              if (!ensure_vertices (instr->vertices.many_buffers[i], data->error))
                {
                  data->failure = TRUE;
                  return TRUE;
                }
            }
        }
      else if (!ensure_vertices (instr->vertices.one_buffer, data->error))
        {
          data->failure = TRUE;
          return TRUE;
        }
      break;
    case CG_PRIV_INSTR_BLIT:
      if (!ensure_texture (instr->blit.src, data->error))
        {
          data->failure = TRUE;
          return TRUE;
        }
      break;
    default:
      g_assert_not_reached ();
    }

  return FALSE;
}

static CgCommands *
plan_unref_to_commands (
    CgPlan *self,
    GError **error)
{
  CglPlan *gl_plan = (CglPlan *)self;
  CglGpu *gl_gpu = (CglGpu *)self->gpu;
  g_autoptr (CgCommands) commands = NULL;

  if (g_atomic_ref_count_dec (&gl_plan->refcount))
    {
      CglCommands *gl_commands = NULL;
      EnsureData data = { 0 };
      guint depth = 0;

      commands = cg_priv_commands_new (self->gpu);
      gl_commands = (CglCommands *)commands;

      /* Simply grab the plan tree for now, no further compilation. */
      gl_commands->instrs = g_steal_pointer (&self->root_instr);

      data.failure = FALSE;
      data.error = error;

      g_node_traverse (
          gl_commands->instrs, G_PRE_ORDER, G_TRAVERSE_ALL,
          -1, (GNodeTraverseFunc)ensure_instr_node, &data);

      /* Plus two so we have enough for blits */
      depth = g_node_max_height (gl_commands->instrs) + 2;

      if (depth > gl_gpu->framebuffer_stack->len)
        {
          guint old_len = 0;

          old_len = gl_gpu->framebuffer_stack->len;
          g_array_set_size (gl_gpu->framebuffer_stack, depth);
          glGenFramebuffers (depth - old_len, &g_array_index (gl_gpu->framebuffer_stack, GLuint, old_len));

          for (guint i = old_len; i < depth; i++)
            {
              if (g_array_index (gl_gpu->framebuffer_stack, GLuint, i) == 0)
                {
                  CGL_SET_ERROR (
                      error, CG_ERROR_FAILED_TARGET_CREATION,
                      "Failed to generate framebuffer");
                  return NULL;
                }
            }
        }

      clear_plan (self);
      g_free (self);

      if (data.failure)
        return NULL;
    }
  else
    {
      CGL_CRITICAL_USER_ERROR (
          "Plan object still has references elsewhere, "
          "so its resources cannot be compiled!");
      return NULL;
    }

  return g_steal_pointer (&commands);
}

typedef struct
{
  CgCommands *commands;
  GLint framebuffer;
  gboolean failure;
  GError **error;
} ProcessData;

static const GLenum gl_draw_buffer_enums[] = {
  GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
  GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
  GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7, GL_COLOR_ATTACHMENT8,
  GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11,
  GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14,
  GL_COLOR_ATTACHMENT15, GL_COLOR_ATTACHMENT16, GL_COLOR_ATTACHMENT17,
  GL_COLOR_ATTACHMENT18, GL_COLOR_ATTACHMENT19, GL_COLOR_ATTACHMENT20,
  GL_COLOR_ATTACHMENT21, GL_COLOR_ATTACHMENT22, GL_COLOR_ATTACHMENT23,
  GL_COLOR_ATTACHMENT24, GL_COLOR_ATTACHMENT25, GL_COLOR_ATTACHMENT26,
  GL_COLOR_ATTACHMENT27, GL_COLOR_ATTACHMENT28, GL_COLOR_ATTACHMENT29,
  GL_COLOR_ATTACHMENT30, GL_COLOR_ATTACHMENT31
};

static const GLenum gl_texture_slot_enums[] = {
  GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2,
  GL_TEXTURE3, GL_TEXTURE4, GL_TEXTURE5,
  GL_TEXTURE6, GL_TEXTURE7, GL_TEXTURE8,
  GL_TEXTURE9, GL_TEXTURE10, GL_TEXTURE11,
  GL_TEXTURE12, GL_TEXTURE13, GL_TEXTURE14,
  GL_TEXTURE15, GL_TEXTURE16, GL_TEXTURE17,
  GL_TEXTURE18, GL_TEXTURE19, GL_TEXTURE20,
  GL_TEXTURE21, GL_TEXTURE22, GL_TEXTURE23,
  GL_TEXTURE24, GL_TEXTURE25, GL_TEXTURE26,
  GL_TEXTURE27, GL_TEXTURE28, GL_TEXTURE29,
  GL_TEXTURE30, GL_TEXTURE31
};

static gboolean
setup_or_teardown (GLuint framebuffer,
                   GLuint blit_read_fb,
                   GLuint blit_draw_fb,
                   CgPrivInstr *instr,
                   CgPrivInstr *pass_instr,
                   ProcessData *data,
                   gboolean teardown)
{
  CgShader *shader = pass_instr->pass.shader;
  CglShader *gl_shader = (CglShader *)shader;

  glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
  glUseProgram (gl_shader->program);

  if (pass_instr->pass.dest[0] >= 0)
    glViewport (pass_instr->pass.dest[0],
                pass_instr->pass.dest[1],
                pass_instr->pass.dest[2],
                pass_instr->pass.dest[3]);

  glColorMask (
      pass_instr->pass.write_mask & CG_WRITE_MASK_COLOR_RED ? GL_TRUE : GL_FALSE,
      pass_instr->pass.write_mask & CG_WRITE_MASK_COLOR_GREEN ? GL_TRUE : GL_FALSE,
      pass_instr->pass.write_mask & CG_WRITE_MASK_COLOR_BLUE ? GL_TRUE : GL_FALSE,
      pass_instr->pass.write_mask & CG_WRITE_MASK_COLOR_ALPHA ? GL_TRUE : GL_FALSE);
  glDepthMask (
      pass_instr->pass.write_mask & CG_WRITE_MASK_DEPTH ? GL_TRUE : GL_FALSE);

  for (guint i = 0, colors = 0, depths = 0;
       i < pass_instr->pass.targets->len;
       i++)
    {
      CgTexture *target = NULL;
      CglTexture *gl_target = NULL;

      target = g_ptr_array_index (pass_instr->pass.targets, i);
      gl_target = (CglTexture *)target;

      if (target->init.format == CG_PRIV_FORMAT_DEPTH)
        {
          g_assert (depths == 0);

          glFramebufferTexture2D (
              GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
              target->init.msaa > 0
                  ? GL_TEXTURE_2D_MULTISAMPLE
                  : GL_TEXTURE_2D,
              teardown
                  ? 0
                  : gl_target->id,
              0);
          depths++;
        }
      else
        {
          g_assert (colors < G_N_ELEMENTS (gl_draw_buffer_enums));

          glFramebufferTexture2D (
              GL_FRAMEBUFFER, gl_draw_buffer_enums[colors],
              target->init.msaa > 0
                  ? GL_TEXTURE_2D_MULTISAMPLE
                  : GL_TEXTURE_2D,
              teardown
                  ? 0
                  : gl_target->id,
              0);
          colors++;
        }
    }

  glDrawBuffers (
      CLAMP (pass_instr->pass.targets->len, 1, G_N_ELEMENTS (gl_draw_buffer_enums)),
      gl_draw_buffer_enums);

  if (!teardown)
    {
      GLenum status = 0;

      status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
        {
          CGL_SET_ERROR (
              data->error,
              CG_ERROR_FAILED_TARGET_CREATION,
              "Failed to complete framebuffer");
          data->failure = TRUE;
          return FALSE;
        }

      glClearColor (0, 0, 0, 0);
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

  for (guint i = 0, textures = 0;
       i < pass_instr->pass.uniforms.order->len;
       i++)
    {
      const char *name = NULL;
      CgValue *value = NULL;
      guint uniform_index = 0;
      ShaderLocation *uniform = NULL;

      name = g_ptr_array_index (pass_instr->pass.uniforms.order, i);
      value = g_hash_table_lookup (pass_instr->pass.uniforms.hash, name);

      uniform_index = GPOINTER_TO_UINT (g_hash_table_lookup (gl_shader->uniform_assoc, name));
      g_assert (uniform_index > 0);
      uniform = &gl_shader->uniforms[uniform_index - 1];

      g_assert (value != NULL);
      g_assert (uniform != NULL);

      switch (value->type)
        {
        case CG_TYPE_TEXTURE:
          {
            GLint textures_int = textures;
            CglTexture *gl_texture = (CglTexture *)value->texture;

            g_assert (textures < G_N_ELEMENTS (gl_texture_slot_enums));

            if (value->texture->init.msaa > 0)
              {
                CglTexture *read_texture = NULL;

                read_texture = gl_texture;
                gl_texture = (CglTexture *)gl_texture->non_msaa;

                /* If the texture uses msaa, blit to a temp texture */
                if (!teardown)
                  {
                    for (int j = 0; j < 2; j++)
                      {
                        GLenum status = 0;

                        glBindFramebuffer (
                            GL_FRAMEBUFFER,
                            j == 0
                                ? blit_read_fb
                                : blit_draw_fb);

                        glFramebufferTexture2D (
                            GL_FRAMEBUFFER,
                            value->texture->init.format == CG_PRIV_FORMAT_DEPTH
                                ? GL_DEPTH_ATTACHMENT
                                : GL_COLOR_ATTACHMENT0,
                            (j == 0 ? read_texture->non_msaa : gl_texture->non_msaa) != NULL
                                ? GL_TEXTURE_2D_MULTISAMPLE
                                : GL_TEXTURE_2D,
                            j == 0
                                ? read_texture->id
                                : gl_texture->id,
                            0);

                        status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
                        if (status != GL_FRAMEBUFFER_COMPLETE)
                          {
                            CGL_SET_ERROR (
                                data->error,
                                CG_ERROR_FAILED_TARGET_CREATION,
                                "Failed to complete framebuffer");
                            return FALSE;
                          }
                      }

                    glBindFramebuffer (GL_READ_FRAMEBUFFER, blit_read_fb);
                    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, blit_draw_fb);
                    glBlitFramebuffer (
                        0, 0, value->texture->init.width, value->texture->init.height,
                        0, 0, value->texture->init.width, value->texture->init.height,
                        value->texture->init.format == CG_PRIV_FORMAT_DEPTH
                            ? GL_DEPTH_BUFFER_BIT
                            : GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);

                    for (int j = 0; j < 2; j++)
                      {
                        glBindFramebuffer (
                            GL_FRAMEBUFFER,
                            j == 0
                                ? blit_read_fb
                                : blit_draw_fb);

                        glFramebufferTexture2D (
                            GL_FRAMEBUFFER,
                            value->texture->init.format == CG_PRIV_FORMAT_DEPTH
                                ? GL_DEPTH_ATTACHMENT
                                : GL_COLOR_ATTACHMENT0,
                            (j == 0 ? read_texture->non_msaa : gl_texture->non_msaa) != NULL
                                ? GL_TEXTURE_2D_MULTISAMPLE
                                : GL_TEXTURE_2D,
                            0, 0);
                      }

                    glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
                    glUseProgram (gl_shader->program);
                  }
              }

            glActiveTexture (gl_texture_slot_enums[textures]);
            glBindTexture (value->texture->init.cubemap
                               ? GL_TEXTURE_CUBE_MAP
                               : GL_TEXTURE_2D,
                           teardown
                               ? 0
                               : gl_texture->id);
            glUniform1iv (uniform->location, 1, &textures_int);
            glActiveTexture (gl_texture_slot_enums[0]);

            textures++;
          }
          break;
        case CG_TYPE_BUFFER:
          {
            guint block_index = 0;
            CglBuffer *gl_buffer = (CglBuffer *)value->buffer;

            block_index = GPOINTER_TO_UINT (
                g_hash_table_lookup (
                    gl_shader->uniform_blocks,
                    GUINT_TO_POINTER (uniform->location)));
            g_assert (block_index > 0);

            glUniformBlockBinding (gl_shader->program, block_index - 1, 0);
            glBindBufferBase (GL_UNIFORM_BUFFER, 0, teardown ? 0 : gl_buffer->ubo_id);
          }
          break;
        case CG_TYPE_BOOL:
          if (!teardown)
            glUniform1i (uniform->location, value->b ? GL_TRUE : GL_FALSE);
          break;
        case CG_TYPE_INT:
          if (!teardown)
            glUniform1i (uniform->location, value->i);
          break;
        case CG_TYPE_UINT:
          if (!teardown)
            glUniform1ui (uniform->location, value->ui);
          break;
        case CG_TYPE_FLOAT:
          if (!teardown)
            glUniform1f (uniform->location, value->f);
          break;
        case CG_TYPE_VEC2:
          if (!teardown)
            glUniform2fv (uniform->location, 1, value->vec2);
          break;
        case CG_TYPE_VEC3:
          if (!teardown)
            glUniform3fv (uniform->location, 1, value->vec3);
          break;
        case CG_TYPE_VEC4:
          if (!teardown)
            glUniform4fv (uniform->location, 1, value->vec4);
          break;
        case CG_TYPE_MAT4:
          if (!teardown)
            glUniformMatrix4fv (uniform->location, 1, GL_FALSE, value->mat4.initialized);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return TRUE;
}

static void
draw_vertices (CgBuffer **buffers,
               guint n_buffers,
               CgShader *shader,
               guint instances)
{
  CglShader *gl_shader = (CglShader *)shader;
  CglBuffer *first_buffer = (CglBuffer *)*buffers;
  guint max_length = 0;

  glBindVertexArray (first_buffer->vao_id);

  for (guint i = 0; i < n_buffers; i++)
    {
      CglBuffer *gl_buffer = NULL;
      gsize stride = 0;
      gsize offset = 0;

      g_assert (buffers[i]->spec != NULL);

      gl_buffer = (CglBuffer *)buffers[i];
      glBindBuffer (GL_ARRAY_BUFFER, gl_buffer->vbo_id);

      for (guint j = 0; j < buffers[i]->spec_length; j++)
        stride += buffers[i]->spec[j].num
                  * (buffers[i]->spec[j].type == CG_TYPE_FLOAT
                         ? sizeof (float)
                         : sizeof (guchar));

      for (guint j = 0; j < buffers[i]->spec_length; j++)
        {
          ShaderLocation *attribute = NULL;

          attribute = g_hash_table_lookup (
              gl_shader->attribute_assoc,
              buffers[i]->spec[j].name);
          g_assert (attribute != NULL);

          glVertexAttribPointer (
              attribute->location,
              buffers[i]->spec[j].num,
              buffers[i]->spec[j].type == CG_TYPE_FLOAT
                  ? GL_FLOAT
                  : GL_UNSIGNED_BYTE,
              GL_FALSE,
              stride,
              GSIZE_TO_POINTER (offset));
          glVertexAttribDivisor (
              attribute->location,
              buffers[i]->spec[j].instance_rate);
          glEnableVertexAttribArray (attribute->location);

          offset += buffers[i]->spec[j].num
                    * (buffers[i]->spec[j].type == CG_TYPE_FLOAT
                           ? sizeof (float)
                           : sizeof (guchar));
        }

      gl_buffer->length = buffers[i]->init.size / stride;
      if (gl_buffer->length > max_length)
        max_length = gl_buffer->length;
    }

  if (instances > 1)
    glDrawArraysInstanced (GL_TRIANGLES, 0, max_length, instances);
  else
    glDrawArrays (GL_TRIANGLES, 0, max_length);

  for (guint i = 0; i < n_buffers; i++)
    {
      CglBuffer *gl_buffer = NULL;

      g_assert (buffers[i]->spec != NULL);

      gl_buffer = (CglBuffer *)buffers[i];
      glBindBuffer (GL_ARRAY_BUFFER, gl_buffer->vbo_id);

      for (guint j = 0; j < buffers[i]->spec_length; j++)
        {
          ShaderLocation *attribute = NULL;

          attribute = g_hash_table_lookup (
              gl_shader->attribute_assoc,
              buffers[i]->spec[j].name);
          g_assert (attribute != NULL);

          glDisableVertexAttribArray (attribute->location);
        }
    }

  glBindVertexArray (0);
}

static gboolean
process_instr_node (GNode *node,
                    gpointer user_data)
{
  CgPrivInstr *instr = node->data;
  ProcessData *data = user_data;
  CgCommands *commands = data->commands;
  CglGpu *gl_gpu = (CglGpu *)commands->gpu;
  CgPrivInstr *pass_instr = NULL;
  GLuint framebuffer = 0;
  GLuint blit_read_fb = 0;
  GLuint blit_draw_fb = 0;
  CgShader *shader = NULL;
  CglShader *gl_shader = NULL;

  if (instr->type == CG_PRIV_INSTR_PASS)
    return FALSE;

  g_assert (node->parent != NULL);
  pass_instr = node->parent->data;
  shader = pass_instr->pass.shader;
  gl_shader = (CglShader *)shader;

  if (pass_instr->pass.targets->len == 0)
    framebuffer = data->framebuffer;
  else
    framebuffer = g_array_index (gl_gpu->framebuffer_stack, GLuint, pass_instr->depth);

  blit_read_fb = g_array_index (gl_gpu->framebuffer_stack, GLuint, pass_instr->depth + 1);
  blit_draw_fb = g_array_index (gl_gpu->framebuffer_stack, GLuint, pass_instr->depth + 2);

  if (node->prev == NULL
      && !setup_or_teardown (
          framebuffer, blit_read_fb, blit_draw_fb,
          instr, pass_instr, data, FALSE))
    return TRUE;

  if (node->prev != NULL
      && ((CgPrivInstr *)node->prev->data)->type
             == CG_PRIV_INSTR_PASS)
    {
      glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
      glUseProgram (gl_shader->program);
    }

  switch (instr->type)
    {
    case CG_PRIV_INSTR_VERTICES:
      if (instr->vertices.n_buffers > 1)
        draw_vertices (
            instr->vertices.many_buffers,
            instr->vertices.n_buffers,
            shader, instr->vertices.instances);
      else
        draw_vertices (
            &instr->vertices.one_buffer, 1,
            shader, instr->vertices.instances);
      break;
    case CG_PRIV_INSTR_BLIT:
      {
        CglTexture *gl_texture = (CglTexture *)instr->blit.src;
        GLenum status = 0;

        glBindFramebuffer (GL_FRAMEBUFFER, blit_read_fb);
        glFramebufferTexture2D (
            GL_FRAMEBUFFER,
            instr->blit.src->init.format == CG_PRIV_FORMAT_DEPTH
                ? GL_DEPTH_ATTACHMENT
                : GL_COLOR_ATTACHMENT0,
            instr->blit.src->init.msaa > 0
                ? GL_TEXTURE_2D_MULTISAMPLE
                : GL_TEXTURE_2D,
            gl_texture->id, 0);

        status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
          {
            CGL_SET_ERROR (
                data->error,
                CG_ERROR_FAILED_TARGET_CREATION,
                "Failed to complete framebuffer");
            return FALSE;
          }

        glBindFramebuffer (GL_READ_FRAMEBUFFER, blit_read_fb);
        glBindFramebuffer (GL_DRAW_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer (
            0, 0, instr->blit.src->init.width, instr->blit.src->init.height,
            pass_instr->pass.dest[0], pass_instr->pass.dest[1],
            pass_instr->pass.dest[2], pass_instr->pass.dest[3],
            instr->blit.src->init.format == CG_PRIV_FORMAT_DEPTH
                ? GL_DEPTH_BUFFER_BIT
                : GL_COLOR_BUFFER_BIT,
            GL_NEAREST);

        glFramebufferTexture2D (
            GL_FRAMEBUFFER,
            instr->blit.src->init.format == CG_PRIV_FORMAT_DEPTH
                ? GL_DEPTH_ATTACHMENT
                : GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, 0, 0);

        glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
        glUseProgram (gl_shader->program);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  if (node->next == NULL
      && !setup_or_teardown (
          framebuffer, blit_read_fb, blit_draw_fb,
          instr, pass_instr, data, TRUE))
    return TRUE;

  return FALSE;
}

static gboolean
commands_dispatch (
    CgCommands *self,
    GError **error)
{
  ProcessData data = { 0 };
  CglCommands *gl_commands = (CglCommands *)self;

  gpu_flush (self->gpu, error);

  data.commands = self;
  glGetIntegerv (GL_FRAMEBUFFER_BINDING, &data.framebuffer);
  data.error = error;
  data.failure = FALSE;

  g_node_traverse (
      gl_commands->instrs, G_PRE_ORDER, G_TRAVERSE_ALL,
      -1, process_instr_node, &data);

  return !data.failure;
}

const CgBackendImpl cg_gl_impl = {
  .is_threadsafe = FALSE,
  .get_gpu_for_this_thread = get_gpu_for_this_thread,
  .set_gpu_for_this_thread = set_gpu_for_this_thread,

  .gpu_new = gpu_new,
  .gpu_ref = gpu_ref,
  .gpu_unref = gpu_unref,
  .gpu_get_info = gpu_get_info,
  .gpu_flush = gpu_flush,

  .plan_new = plan_new,
  .plan_ref = plan_ref,
  .plan_unref = plan_unref,

  .shader_new = shader_new,
  .shader_ref = shader_ref,
  .shader_unref = shader_unref,

  .buffer_new = buffer_new,
  .buffer_ref = buffer_ref,
  .buffer_unref = buffer_unref,

  .texture_new = texture_new,
  .texture_ref = texture_ref,
  .texture_unref = texture_unref,

  .commands_new = commands_new,
  .commands_ref = commands_ref,
  .commands_unref = commands_unref,

  .plan_unref_to_commands = plan_unref_to_commands,
  .commands_dispatch = commands_dispatch,
};
