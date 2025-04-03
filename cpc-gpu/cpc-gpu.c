/* cpc-gpu.c
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

#define G_LOG_DOMAIN "CpcGpu"
#include "cpc-gpu-private.h"

#define CG_PRIV_GATHER_VA_ARGS_INTO(first, arr, c_arr, n_arr) \
  G_STMT_START                                                \
  {                                                           \
    va_list var_args = { 0 };                                 \
    typeof (first) tmp = first;                               \
    va_start (var_args, first);                               \
    for (;;)                                                  \
      {                                                       \
        (arr)[(n_arr)++] = tmp;                               \
        if ((n_arr) >= (c_arr))                               \
          {                                                   \
            CG_PRIV_CRITICAL (                                \
                "Too many arguments were passed "             \
                "to variadic function. The max was "          \
                "%d trailing arguments. Input "               \
                "will be truncated.",                         \
                (int)c_arr);                                  \
            break;                                            \
          }                                                   \
        tmp = va_arg (var_args, typeof (first));              \
        if (tmp == NULL) break;                               \
      }                                                       \
    va_end (var_args);                                        \
  }                                                           \
  G_STMT_END

#define CG_PRIV_ENTER(gpu) CG_PRIV_ENTER_BIT (gpu, CG_PRIV_DATA_LOCK_BIT)
#define CG_PRIV_LEAVE(gpu) CG_PRIV_LEAVE_BIT (gpu, CG_PRIV_DATA_LOCK_BIT)

#define CG_PRIV_HAS_THREAD(gpu) ( \
    (gpu)->impl->is_threadsafe    \
    || !(gpu)->threadsafe         \
    || (gpu) == (gpu)->impl->get_gpu_for_this_thread ())
#define CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL(gpu, val)                         \
  G_STMT_START                                                                \
  {                                                                           \
    CG_PRIV_ENTER (gpu);                                                      \
    if (!CG_PRIV_HAS_THREAD (gpu))                                            \
      {                                                                       \
        CG_PRIV_LEAVE (gpu);                                                  \
        CG_PRIV_CRITICAL ("GPU does not own the current thread. Returning!"); \
        return (val);                                                         \
      }                                                                       \
  }                                                                           \
  G_STMT_END

#define CG_PRIV_HANDLE_BACKEND_ERROR(success_cond, error, steal_error, gpu, return_val) \
  G_STMT_START                                                                          \
  {                                                                                     \
    if (!(success_cond))                                                                \
      {                                                                                 \
        if ((gpu)->debug_output)                                                        \
          {                                                                             \
            if ((steal_error) != NULL)                                                  \
              CG_PRIV_CRITICAL ("Backend reported an error: %s",                        \
                                (steal_error)->message);                                \
            else                                                                        \
              CG_PRIV_CRITICAL ("Backend reported a user error.");                      \
          }                                                                             \
        if ((gpu)->exit_on_error)                                                       \
          CG_PRIV_FATAL (                                                               \
              "The check `%s` did not pass and GPU has been configured to exit.",       \
              G_STRINGIFY (success_cond));                                              \
        g_propagate_error ((error), g_steal_pointer (&(steal_error)));                  \
        return (return_val);                                                            \
      }                                                                                 \
  }                                                                                     \
  G_STMT_END

/* clang-format off */
G_DEFINE_QUARK (cg-error-quark, cg_error);
/* clang-format on */

#define GL_ENUM_STR G_STRINGIFY (CG_INIT_FLAG_BACKEND_VULKAN)
#define VK_ENUM_STR G_STRINGIFY (CG_INIT_FLAG_BACKEND_OPENGL)

CgGpu *
cg_gpu_new (guint32 flags,
            gpointer extra_data,
            GError **error)
{
  const CgBackendImpl *impl = NULL;
  const char *enum_str = NULL;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (CgGpu) gpu = NULL;

  if (flags & CG_INIT_FLAG_BACKEND_VULKAN)
    {
      // impl = &cg_vk_impl;
      enum_str = VK_ENUM_STR;

      CG_PRIV_CRITICAL (
          "%s: Cannot initialize Vulkan "
          "backend: not implemented yet",
          VK_ENUM_STR);
    }
  else if (flags & CG_INIT_FLAG_BACKEND_OPENGL)
    {
      impl = &cg_gl_impl;
      enum_str = GL_ENUM_STR;
    }
  else
    CG_PRIV_CRITICAL (
        "Cannot initialize backend. Please pass "
        "the flag " GL_ENUM_STR " or " VK_ENUM_STR);

  g_return_val_if_fail (impl != NULL, NULL);

  gpu = impl->gpu_new (flags, extra_data, &local_error);

  if (gpu == NULL)
    {
      CG_PRIV_CRITICAL (
          "Could not initialize backend %s: %s",
          enum_str, local_error->message);

      if (flags & CG_INIT_FLAG_EXIT_ON_ERROR)
        g_assert_not_reached ();

      g_propagate_error (error, g_steal_pointer (&local_error));
      return NULL;
    }

  gpu->impl = impl;
  gpu->threadsafe = !(flags & CG_INIT_FLAG_NO_THREAD_SAFETY);
  gpu->debug_output = flags & CG_INIT_FLAG_LOG_ERRORS;
  gpu->exit_on_error = flags & CG_INIT_FLAG_EXIT_ON_ERROR;

  return g_steal_pointer (&gpu);
}

CgGpu *
cg_gpu_ref (CgGpu *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->impl->gpu_ref (self);
}

void
cg_gpu_unref (gpointer self)
{
  CgGpu *gpu = NULL;

  g_return_if_fail (self != NULL);

  gpu = self;
  gpu->impl->gpu_unref (self);
}

char *
cg_gpu_get_info (CgGpu *self,
                 const char *param,
                 GError **error)
{
  GError *local_error = NULL;
  char *info = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (param != NULL, NULL);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self, FALSE);
  info = self->impl->gpu_get_info (self, param, &local_error);
  CG_PRIV_LEAVE (self);

  CG_PRIV_HANDLE_BACKEND_ERROR (info != NULL, error, local_error, self, NULL);

  return info;
}

gboolean
cg_gpu_steal_this_thread (CgGpu *self)
{
  gboolean was_set = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  if (!CG_PRIV_DEAL_WITH_THREADS (self))
    return TRUE;

  CG_PRIV_ENTER (self);
  if (self != self->impl->get_gpu_for_this_thread ())
    {
      self->impl->set_gpu_for_this_thread (self);
      was_set = TRUE;
    }
  CG_PRIV_LEAVE (self);

  return was_set;
}

gboolean
cg_gpu_flush (CgGpu *self, GError **error)
{
  GError *local_error = NULL;
  gboolean success = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self, FALSE);
  success = self->impl->gpu_flush (self, &local_error);
  CG_PRIV_LEAVE (self);

  CG_PRIV_HANDLE_BACKEND_ERROR (success, error, local_error, self, FALSE);

  return TRUE;
}

static void
plan_init (CgPlan *self)
{
  self->root_instr = NULL;
  self->cur_instr = NULL;
  self->configuring = NULL;
}

static gboolean
destroy_instr_node_data (GNode *node,
                         gpointer user_data)
{
  CgPrivInstr *instr = node->data;

  switch (instr->type)
    {
    case CG_PRIV_INSTR_PASS:
      g_clear_pointer (&instr->pass.shader, cg_shader_unref);
      g_clear_pointer (&instr->pass.targets, g_ptr_array_unref);
      g_clear_pointer (&instr->pass.attributes, g_hash_table_unref);
      g_clear_pointer (&instr->pass.uniforms.hash, g_hash_table_unref);
      g_clear_pointer (&instr->pass.uniforms.order, g_ptr_array_unref);
      break;
    case CG_PRIV_INSTR_VERTICES:
      g_clear_pointer (&instr->vertices, g_ptr_array_unref);
      break;
    }

  if (instr->user_data != NULL
      && instr->destroy_user_data != NULL)
    instr->destroy_user_data (instr->user_data);

  g_free (instr);

  /* Keep going */
  return FALSE;
}

void
cg_priv_destroy_instr_node (GNode *self)
{
  g_node_traverse (
      self, G_PRE_ORDER, G_TRAVERSE_ALL,
      -1, destroy_instr_node_data, NULL);
  g_node_destroy (self);
}

void
cg_priv_plan_finish (CgPlan *self)
{
  g_clear_pointer (&self->root_instr, cg_priv_destroy_instr_node);
  g_clear_pointer (&self->gpu, cg_gpu_unref);
}

static void
shader_init (CgShader *self)
{
}

void
cg_priv_shader_finish (CgShader *self)
{
  g_clear_pointer (&self->init.fragment_code, g_free);
  g_clear_pointer (&self->init.vertex_code, g_free);
  g_clear_pointer (&self->gpu, cg_gpu_unref);
}

static void
buffer_init (CgBuffer *self)
{
}

void
cg_priv_buffer_finish (CgBuffer *self)
{
  g_clear_pointer (&self->init.data, g_free);
  clear_data_layout (self->pending_layout, self->pending_layout_len);
  g_clear_pointer (&self->pending_layout, g_free);
  g_clear_pointer (&self->gpu, cg_gpu_unref);
}

static void
texture_init (CgTexture *self)
{
}

void
cg_priv_texture_finish (CgTexture *self)
{
  g_clear_pointer (&self->init.data, g_free);
  g_clear_pointer (&self->gpu, cg_gpu_unref);
}

static void
commands_init (CgCommands *self)
{
}

void
cg_priv_commands_finish (CgCommands *self)
{
  g_clear_pointer (&self->debug_output, g_free);
  g_clear_pointer (&self->gpu, cg_gpu_unref);
}

#define RELAY_IMPL_OBJECT(lower, upper)                   \
  static Cg##upper *lower##_new (CgGpu *gpu)              \
  {                                                       \
    g_autoptr (Cg##upper) obj = NULL;                     \
    obj = gpu->impl->lower##_new (gpu);                   \
    obj->gpu = cg_gpu_ref (gpu);                          \
    lower##_init (obj);                                   \
    return g_steal_pointer (&obj);                        \
  }                                                       \
  Cg##upper *cg_##lower##_ref (Cg##upper *self)           \
  {                                                       \
    g_return_val_if_fail (self != NULL, NULL);            \
    return self->gpu->impl->lower##_ref (self);           \
  }                                                       \
  void cg_##lower##_unref (gpointer self)                 \
  {                                                       \
    g_return_if_fail (self != NULL);                      \
    ((Cg##upper *)self)->gpu->impl->lower##_unref (self); \
  }

RELAY_IMPL_OBJECT (shader, Shader);
RELAY_IMPL_OBJECT (buffer, Buffer);
RELAY_IMPL_OBJECT (texture, Texture);

RELAY_IMPL_OBJECT (plan, Plan);
CgPlan *
cg_plan_new (CgGpu *self)
{
  return plan_new (self);
}

RELAY_IMPL_OBJECT (commands, Commands);
CgCommands *
cg_priv_commands_new (CgGpu *gpu)
{
  return commands_new (gpu);
}

#undef RELAY_IMPL_OBJECT

CgShader *
cg_shader_new_for_code (
    CgGpu *self,
    const char *vertex_code,
    const char *fragment_code)
{
  g_autoptr (CgShader) shader = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (vertex_code != NULL, NULL);
  g_return_val_if_fail (fragment_code != NULL, NULL);

  shader = shader_new (self);

  shader->init.vertex_code = g_strdup (vertex_code);
  shader->init.fragment_code = g_strdup (fragment_code);

  return g_steal_pointer (&shader);
}

CgBuffer *
cg_buffer_new_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size)
{
  g_autoptr (CgBuffer) buffer = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);

  buffer = buffer_new (self);
  buffer->init.data = g_memdup2 (data, size);
  buffer->init.size = size;

  return g_steal_pointer (&buffer);
}

CgBuffer *
cg_buffer_new_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size)
{
  g_autoptr (CgBuffer) buffer = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);

  buffer = buffer_new (self);
  buffer->init.data = data;
  buffer->init.size = size;

  return g_steal_pointer (&buffer);
}

void
cg_buffer_hint_layout (
    CgBuffer *self,
    const CgDataSegment *segments,
    guint n_segments)
{
  g_autofree CgDataSegment *segments_dup = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (segments != NULL);
  g_return_if_fail (n_segments > 0);

  segments_dup = g_malloc0_n (n_segments, sizeof (*segments));
  for (guint i = 0; i < n_segments; i++)
    {
      segments_dup[i].name = g_strdup (segments[i].name);
      segments_dup[i].num = segments[i].num;
      segments_dup[i].type = segments[i].type;
    }

  clear_data_layout (self->pending_layout, self->pending_layout_len);
  CG_PRIV_REPLACE_POINTER (&self->pending_layout, g_steal_pointer (&segments_dup), g_free);
  self->pending_layout_len = n_segments;
}

CgTexture *
cg_texture_new_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    int width,
    int height,
    int format,
    int mipmaps,
    int msaa)
{
  g_autoptr (CgTexture) texture = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data == NULL || size > 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (format > CG_FORMAT_0 && format < CG_N_FORMATS, NULL);
  g_return_val_if_fail (mipmaps >= 0, NULL);
  g_return_val_if_fail (msaa >= 0, NULL);

  texture = texture_new (self);

  texture->init.cubemap = FALSE;
  texture->init.data = data != NULL ? g_memdup2 (data, size) : NULL;
  texture->init.width = width;
  texture->init.height = height;
  texture->init.format = format;
  texture->init.mipmaps = mipmaps;
  texture->init.msaa = msaa;

  return g_steal_pointer (&texture);
}

CgTexture *
cg_texture_new_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    int width,
    int height,
    int format,
    int mipmaps,
    int msaa)
{
  g_autoptr (CgTexture) texture = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (format > CG_FORMAT_0 && format < CG_N_FORMATS, NULL);
  g_return_val_if_fail (mipmaps >= 0, NULL);
  g_return_val_if_fail (msaa >= 0, NULL);

  texture = texture_new (self);

  texture->init.cubemap = FALSE;
  texture->init.data = data;
  texture->init.width = width;
  texture->init.height = height;
  texture->init.format = format;
  texture->init.mipmaps = mipmaps;
  texture->init.msaa = msaa;

  return g_steal_pointer (&texture);
}

CgTexture *
cg_texture_new_cubemap_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    int image_size,
    int format)
{
  g_autoptr (CgTexture) texture = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (image_size > 0, NULL);
  g_return_val_if_fail (format > CG_FORMAT_0 && format < CG_N_FORMATS, NULL);

  texture = texture_new (self);

  texture->init.cubemap = TRUE;
  texture->init.data = g_memdup2 (data, size);
  texture->init.width = image_size;
  texture->init.height = image_size;
  texture->init.format = format;
  texture->init.mipmaps = 0;
  texture->init.msaa = 0;

  return g_steal_pointer (&texture);
}

CgTexture *
cg_texture_new_cubemap_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    int image_size,
    int format)
{
  g_autoptr (CgTexture) texture = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (image_size > 0, NULL);
  g_return_val_if_fail (format > CG_FORMAT_0 && format < CG_N_FORMATS, NULL);

  texture = texture_new (self);

  texture->init.cubemap = TRUE;
  texture->init.data = data;
  texture->init.width = image_size;
  texture->init.height = image_size;
  texture->init.format = format;
  texture->init.mipmaps = 0;
  texture->init.msaa = 0;

  return g_steal_pointer (&texture);
}

CgTexture *
cg_texture_new_depth (
    CgGpu *self,
    int width,
    int height,
    int msaa)
{
  g_autoptr (CgTexture) texture = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (msaa >= 0, NULL);

  texture = texture_new (self);

  texture->init.cubemap = FALSE;
  texture->init.data = NULL;
  texture->init.width = width;
  texture->init.height = height;
  texture->init.format = CG_PRIV_FORMAT_DEPTH;
  texture->init.mipmaps = 0;
  texture->init.msaa = msaa;

  return g_steal_pointer (&texture);
}

void
cg_plan_begin_config (CgPlan *self)
{
  CgPrivInstr *instr = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);

  instr = CG_PRIV_CREATE (instr);
  instr->depth = g_node_max_height (self->root_instr);
  instr->type = CG_PRIV_INSTR_PASS;
  instr->pass.targets = g_ptr_array_new_with_free_func (cg_texture_unref);
  instr->pass.uniforms.hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cg_priv_destroy_value);
  instr->pass.uniforms.order = g_ptr_array_new ();
  instr->pass.attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  instr->pass.write_mask = 0;
  instr->pass.depth_test_func = CG_TEST_FUNC_0;
  instr->pass.dest[0] = -1;

  self->configuring = instr;
}

static void
config_targets (CgPlan *self,
                CgTexture **targets,
                guint n_targets)
{
  GPtrArray *config_targets = NULL;
  guint old_length = 0;

  g_assert (self->configuring != NULL);
  g_assert (self->configuring->type == CG_PRIV_INSTR_PASS);

  config_targets = self->configuring->pass.targets;
  old_length = config_targets->len;
  g_ptr_array_set_size (config_targets, old_length + n_targets);

  for (guint i = 0; i < n_targets; i++)
    g_ptr_array_index (config_targets, old_length + i)
        = cg_texture_ref (targets[i]);
}

void
cg_plan_config_targets (
    CgPlan *self,
    CgTexture *first_target,
    ...)
{
  guint n_targets = 0;
  CgTexture *targets[32] = { 0 };

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (first_target != NULL);

  CG_PRIV_GATHER_VA_ARGS_INTO (
      first_target, targets, G_N_ELEMENTS (targets), n_targets);

  config_targets (self, targets, n_targets);
}

void
cg_plan_config_targets_v (
    CgPlan *self,
    CgTexture **targets,
    guint n_targets)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (targets != NULL);
  g_return_if_fail (n_targets > 0);

  config_targets (self, targets, n_targets);
}

void
cg_plan_config_shader (
    CgPlan *self,
    CgShader *shader)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (shader != NULL);

  CG_PRIV_REPLACE_POINTER_REF (
      &self->configuring->pass.shader, shader,
      cg_shader_ref, cg_shader_unref);
}

static void
add_uniform (CgPlan *self,
             const char *name,
             const CgValue *value)
{
  char *key = NULL;
  CgValue *new_value = NULL;

  key = g_strdup (name);
  new_value = cg_priv_transfer_value_from_static_foreign (
      CG_PRIV_CREATE (new_value),
      value);

  g_hash_table_replace (self->configuring->pass.uniforms.hash, key, new_value);
  g_ptr_array_add (self->configuring->pass.uniforms.order, key);
}

void
cg_plan_config_uniforms (
    CgPlan *self,
    const char *first_name,
    const CgValue *first_value,
    ...)
{
  int n_arr = 0;
  va_list var_args = { 0 };
  const char *key = NULL;
  const CgValue *value = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (first_name != NULL);
  g_return_if_fail (first_value != NULL);

  key = first_name;
  value = first_value;

  va_start (var_args, first_value);
  for (;;)
    {
      add_uniform (self, key, value);

      n_arr++;
      key = va_arg (var_args, const char *);
      if (key == NULL) break;

      value = va_arg (var_args, const CgValue *);
      if (value == NULL)
        {
          CG_PRIV_CRITICAL ("Keyval pair %d does not have a value.", n_arr);
          break;
        }
    }
  va_end (var_args);
}

void
cg_plan_config_uniforms_v (
    CgPlan *self,
    const char **names,
    const CgValue *values,
    guint n_uniforms)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (names != NULL);
  g_return_if_fail (values != NULL);
  g_return_if_fail (n_uniforms > 0);

  for (guint i = 0; i < n_uniforms; i++)
    add_uniform (self, names[i], values + i);
}

void
cg_plan_config_dest (
    CgPlan *self,
    int x,
    int y,
    int width,
    int height)
{
  int *dest = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (width != 0);
  g_return_if_fail (height != 0);

  dest = (int *)self->configuring->pass.dest;

  dest[0] = x;
  dest[1] = y;
  dest[2] = width;
  dest[3] = height;
}

void
cg_plan_config_write_mask (
    CgPlan *self,
    guint32 mask)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  self->configuring->pass.write_mask = mask;
}

void
cg_plan_config_depth_test_func (
    CgPlan *self,
    int func)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (func > CG_TEST_FUNC_0 && func < CG_N_TEST_FUNCS);

  self->configuring->pass.depth_test_func = func;
}

void
cg_plan_push_group (CgPlan *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  if (self->cur_instr == NULL)
    {
      CG_PRIV_REPLACE_POINTER (
          &self->root_instr,
          g_node_new (g_steal_pointer (&self->configuring)),
          cg_priv_destroy_instr_node);
      self->cur_instr = self->root_instr;
    }
  else
    self->cur_instr = g_node_append_data (
        self->cur_instr,
        g_steal_pointer (&self->configuring));
}

static const int state_types[CG_N_STATES] = {
  [CG_STATE_TARGET] = CG_TYPE_TEXTURE,
  [CG_STATE_SHADER] = CG_TYPE_SHADER,
  [CG_STATE_UNIFORM] = CG_TYPE_KEYVAL,
  [CG_STATE_DEST] = CG_TYPE_RECT,
  [CG_STATE_WRITE_MASK] = CG_TYPE_UINT,
  [CG_STATE_DEPTH_FUNC] = CG_TYPE_INT,
};

void
cg_plan_push_state (
    CgPlan *self,
    int first_prop,
    const CgValue *first_value,
    ...)
{
  va_list var_args = { 0 };
  int tmp_key = 0;
  const CgValue *tmp_value = NULL;
  guint n_keyvals = 0;
  int keys[64] = { 0 };
  const CgValue *values[G_N_ELEMENTS (keys)] = { 0 };

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (first_value != NULL);

  tmp_key = first_prop;
  tmp_value = first_value;

  va_start (var_args, first_value);
  for (;;)
    {
      if (tmp_key <= CG_STATE_0 || tmp_key >= CG_N_STATES)
        {
          CG_PRIV_CRITICAL ("Key %d was not recognized as valid.", tmp_key);
          break;
        }
      keys[n_keyvals] = tmp_key;
      values[n_keyvals] = tmp_value;
      n_keyvals++;
      if (n_keyvals >= G_N_ELEMENTS (keys))
        {
          CG_PRIV_CRITICAL (
              "Too many arguments passed "
              "to variadic function. The max was "
              "%d keyval pairs. Input "
              "will be truncated.",
              (int)G_N_ELEMENTS (keys));
          break;
        }
      tmp_key = va_arg (var_args, int);
      if (tmp_key == 0) break;
      tmp_value = va_arg (var_args, const CgValue *);
      if (tmp_value == NULL)
        {
          CG_PRIV_CRITICAL ("Keyval pair %d does not have a value.", (int)n_keyvals);
          break;
        }
    }
  va_end (var_args);
  g_return_if_fail (n_keyvals > 0);

  cg_plan_begin_config (self);

  /* TODO: optimize this */
  for (guint i = 0; i < n_keyvals; i++)
    {
      if (state_types[keys[i]] == values[i]->type)
        {
          switch (keys[i])
            {
            case CG_STATE_SHADER:
              cg_plan_config_shader (self, values[i]->shader);
              break;
            case CG_STATE_TARGET:
              cg_plan_config_targets (self, values[i]->texture, NULL);
              break;
            case CG_STATE_UNIFORM:
              cg_plan_config_uniforms (self, values[i]->keyval.foreign.key, values[i]->keyval.foreign.val, NULL);
              break;
            case CG_STATE_DEST:
              cg_plan_config_dest (
                  self,
                  values[i]->rect[0],
                  values[i]->rect[1],
                  values[i]->rect[2],
                  values[i]->rect[3]);
              break;
            case CG_STATE_WRITE_MASK:
              cg_plan_config_write_mask (self, values[i]->ui);
              break;
            case CG_STATE_DEPTH_FUNC:
              cg_plan_config_depth_test_func (self, values[i]->i);
              break;
            }
        }
      else
        CG_PRIV_CRITICAL (
            "Keyval pair %d is invalid, wanted "
            "value of type %s, got type %s.",
            i, cg_priv_get_type_name (state_types[keys[i]]),
            cg_priv_get_type_name (values[i]->type));
    }

  cg_plan_push_group (self);
}

#define DEFINE_APPEND_OP_INNER_FUNC(name, type_name, type_enum, member, ref, unref) \
  static void                                                                       \
  name (CgPlan *self,                                                               \
        type_name **objects,                                                        \
        guint n_objects)                                                            \
  {                                                                                 \
    GNode *child = NULL;                                                            \
    CgPrivInstr *instr = NULL;                                                      \
    guint old_length = 0;                                                           \
    g_assert (self->configuring == NULL);                                           \
    g_assert (self->cur_instr != NULL);                                             \
    child = g_node_last_child (self->cur_instr);                                    \
    if (child != NULL)                                                              \
      {                                                                             \
        CgPrivInstr *child_instr = child->data;                                     \
        if (child_instr->type == type_enum)                                         \
          instr = child_instr;                                                      \
      }                                                                             \
    if (instr == NULL)                                                              \
      {                                                                             \
        instr = CG_PRIV_CREATE (instr);                                             \
        instr->type = type_enum;                                                    \
        instr->member = g_ptr_array_new_with_free_func ((GDestroyNotify)(unref));   \
        g_node_append_data (self->cur_instr, instr);                                \
      }                                                                             \
    old_length = instr->member->len;                                                \
    g_ptr_array_set_size (instr->member, instr->member->len + n_objects);           \
    for (guint i = 0; i < n_objects; i++)                                           \
      g_ptr_array_index (instr->member, old_length + i)                             \
          = ref (objects[i]);                                                       \
  }

DEFINE_APPEND_OP_INNER_FUNC (
    append_vertices_op_inner, CgBuffer,
    CG_PRIV_INSTR_VERTICES, vertices,
    cg_buffer_ref, cg_buffer_unref)

#undef DEFINE_APPEND_OP_INNER_FUNC

static gboolean
validate_append (CgPlan *self)
{
  GNode *node = NULL;
  gboolean has_shader = FALSE;
  gboolean has_write_mask = FALSE;
  gboolean has_depth_func = FALSE;

  g_assert (self->cur_instr != NULL);
  node = self->cur_instr;

  do
    {
      CgPrivInstr *instr = node->data;

      if (!has_shader) has_shader = instr->pass.shader != NULL;
      if (!has_depth_func && !has_write_mask
          && instr->pass.write_mask > 0
          && !(instr->pass.write_mask & CG_WRITE_MASK_DEPTH))
        {
          has_write_mask = TRUE;
          has_depth_func = TRUE;
        }
      else
        {
          if (!has_write_mask) has_write_mask = instr->pass.write_mask > 0;
          if (!has_depth_func) has_depth_func = instr->pass.depth_test_func > 0;
        }

      if (has_shader && has_write_mask && has_depth_func)
        break;
    }
  while ((node = node->parent) != NULL);

  if (!has_shader) CG_PRIV_CRITICAL ("Invalid append: Needs a shader");
  if (!has_write_mask) CG_PRIV_CRITICAL ("Invalid append: Needs a write mask");
  if (!has_depth_func) CG_PRIV_CRITICAL ("Invalid append: Needs a depth test function");

  return has_shader && has_write_mask && has_depth_func;
}

void
cg_plan_append_vertices (
    CgPlan *self,
    CgBuffer *first_vertices,
    ...)
{
  CgBuffer *vertices[32] = { 0 };
  guint n_vertices = 0;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);
  g_return_if_fail (first_vertices != NULL);
  g_return_if_fail (validate_append (self));

  CG_PRIV_GATHER_VA_ARGS_INTO (
      first_vertices, vertices, G_N_ELEMENTS (vertices), n_vertices);

  append_vertices_op_inner (self, vertices, n_vertices);
}

void
cg_plan_append_vertices_v (
    CgPlan *self,
    CgBuffer **vertices,
    guint n_vertices)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);
  g_return_if_fail (vertices != NULL);
  g_return_if_fail (n_vertices > 0);
  g_return_if_fail (validate_append (self));

  append_vertices_op_inner (self, vertices, n_vertices);
}

void
cg_plan_pop_n_groups (
    CgPlan *self,
    guint n_groups)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);

  for (guint i = 0; i < n_groups; i++)
    {
      if (self->cur_instr == NULL)
        {
          CG_PRIV_CRITICAL ("No more groups to pop!");
          break;
        }

      self->cur_instr = self->cur_instr->parent;
    }
}

CgCommands *
cg_plan_unref_to_commands (
    CgPlan *self,
    GError **error)
{
  g_autoptr (CgGpu) gpu = NULL;
  g_autoptr (CgCommands) commands = NULL;
  g_autoptr (GError) local_error = NULL;

  g_return_val_if_fail (self != NULL, NULL);

  gpu = cg_gpu_ref (self->gpu);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self->gpu, NULL);
  commands = self->gpu->impl->plan_unref_to_commands (self, &local_error);
  CG_PRIV_LEAVE (gpu);

  CG_PRIV_HANDLE_BACKEND_ERROR (commands != NULL, error, local_error, gpu, NULL);

  return g_steal_pointer (&commands);
}

gboolean
cg_commands_dispatch (
    CgCommands *self,
    GError **error)
{
  g_autoptr (GError) local_error = NULL;
  gboolean success = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self->gpu, FALSE);
  success = self->gpu->impl->commands_dispatch (self, &local_error);
  CG_PRIV_LEAVE (self->gpu);

  CG_PRIV_HANDLE_BACKEND_ERROR (success, error, local_error, self->gpu, FALSE);

  return TRUE;
}
