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
  CgGpu *owner = NULL;
  gboolean was_set = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);

  if (!CG_PRIV_DEAL_WITH_THREADS (self))
    return TRUE;

  CG_PRIV_ENTER (self);
  owner = self->impl->get_gpu_for_this_thread ();
  if (self != owner)
    {
      self->impl->set_gpu_for_this_thread (self);
      was_set = TRUE;
    }
  CG_PRIV_LEAVE (self);

  return was_set;
}

void
cg_gpu_release_this_thread (CgGpu *self)
{
  CgGpu *owner = NULL;

  g_return_if_fail (self != NULL);

  if (!CG_PRIV_DEAL_WITH_THREADS (self))
    return;

  CG_PRIV_ENTER (self);
  owner = self->impl->get_gpu_for_this_thread ();
  if (self == owner)
    self->impl->set_gpu_for_this_thread (NULL);
  CG_PRIV_LEAVE (self);
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
}

static gboolean
destroy_instr_node_data (GNode *node,
                         gpointer user_data)
{
  CgPrivInstr *instr = node->data;

  if (instr != NULL)
    {
      switch (instr->type)
        {
        case CG_PRIV_INSTR_PASS:
          g_clear_pointer (&instr->pass.shader, cg_shader_unref);
          g_clear_pointer (&instr->pass.targets, g_array_unref);
          g_clear_pointer (&instr->pass.attributes, g_hash_table_unref);
          g_clear_pointer (&instr->pass.uniforms.hash, g_hash_table_unref);
          g_clear_pointer (&instr->pass.uniforms.order, g_ptr_array_unref);
          break;
        case CG_PRIV_INSTR_VERTICES:
          if (instr->vertices.n_buffers > 1)
            {
              for (guint i = 0; i < instr->vertices.n_buffers; i++)
                cg_buffer_unref (instr->vertices.many_buffers[i]);
              g_free (instr->vertices.many_buffers);
            }
          else
            g_clear_pointer (&instr->vertices.one_buffer, cg_buffer_unref);
          break;
        case CG_PRIV_INSTR_BLIT:
          g_clear_pointer (&instr->blit.src, cg_texture_unref);
          break;
        default:
          g_assert_not_reached ();
        }

      if (instr->user_data != NULL
          && instr->destroy_user_data != NULL)
        instr->destroy_user_data (instr->user_data);

      g_free (instr);
    }

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
  cg_priv_clear_data_layout (self->spec, self->spec_length);
  g_clear_pointer (&self->spec, g_free);
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
  if (self->debug.enabled)
    {
      g_clear_pointer (&self->debug.calls.compile, g_ptr_array_unref);
      g_clear_pointer (&self->debug.calls.run, g_ptr_array_unref);
    }

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

static void
hint_buffer_layout (
    CgBuffer *self,
    const CgDataSegment *spec,
    guint spec_length)
{
  g_autofree CgDataSegment *segments_dup = NULL;

  segments_dup = g_malloc0_n (spec_length, sizeof (*spec));
  for (guint i = 0; i < spec_length; i++)
    {
      segments_dup[i].name = g_strdup (spec[i].name);
      segments_dup[i].num = spec[i].num;
      segments_dup[i].type = spec[i].type;
      segments_dup[i].instance_rate = spec[i].instance_rate;
    }

  cg_priv_clear_data_layout (self->spec, self->spec_length);
  CG_PRIV_REPLACE_POINTER (&self->spec, g_steal_pointer (&segments_dup), g_free);
  self->spec_length = spec_length;
}

CgBuffer *
cg_buffer_new_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    const CgDataSegment *spec,
    guint spec_length)
{
  g_autoptr (CgBuffer) buffer = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (spec != NULL || spec_length == 0, NULL);

  buffer = buffer_new (self);
  buffer->init.data = g_memdup2 (data, size);
  buffer->init.size = size;

  if (spec != NULL)
    hint_buffer_layout (buffer, spec, spec_length);

  return g_steal_pointer (&buffer);
}

CgBuffer *
cg_buffer_new_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    const CgDataSegment *spec,
    guint spec_length)
{
  g_autoptr (CgBuffer) buffer = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (spec != NULL || spec_length == 0, NULL);

  buffer = buffer_new (self);
  buffer->init.data = data;
  buffer->init.size = size;

  if (spec != NULL)
    hint_buffer_layout (buffer, spec, spec_length);

  return g_steal_pointer (&buffer);
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
  instr->depth = self->cur_instr != NULL ? ((CgPrivInstr *)self->cur_instr->data)->depth + 1 : 0;
  instr->type = CG_PRIV_INSTR_PASS;

  instr->pass.targets = g_array_new (FALSE, TRUE, sizeof (CgPrivTarget));
  g_array_set_clear_func (instr->pass.targets, cg_priv_clear_target);
  instr->pass.uniforms.hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cg_priv_destroy_value);
  instr->pass.uniforms.order = g_ptr_array_new ();
  instr->pass.attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  instr->pass.write_mask.val = 0;
  instr->pass.write_mask.set = FALSE;
  instr->pass.depth_test_func.val = CG_TEST_FUNC_0;
  instr->pass.depth_test_func.set = FALSE;
  instr->pass.dest.set = FALSE;
  instr->pass.clockwise_faces.val = FALSE;
  instr->pass.clockwise_faces.set = FALSE;
  instr->pass.backface_cull.val = TRUE;
  instr->pass.backface_cull.set = FALSE;

  self->configuring = instr;
}

static void
config_targets (CgPlan *self,
                const CgValue *const *targets,
                guint n_targets)
{
  guint old_length = 0;

  g_assert (self->configuring != NULL);
  g_assert (self->configuring->type == CG_PRIV_INSTR_PASS);

  old_length = self->configuring->pass.targets->len;
  g_array_set_size (self->configuring->pass.targets, old_length + n_targets);

  for (guint i = 0; i < n_targets; i++)
    {
      CgPrivTarget *target = NULL;

      target = &g_array_index (self->configuring->pass.targets, CgPrivTarget, old_length + i);

      switch (targets[i]->type)
        {
        case CG_TYPE_TEXTURE:
          target->texture = cg_texture_ref (targets[i]->texture);
          target->src_blend = CG_BLEND_SRC_ALPHA;
          target->dst_blend = CG_BLEND_ONE_MINUS_SRC_ALPHA;
          break;
        case CG_TYPE_TUPLE3:
          target->texture = cg_texture_ref (targets[i]->tuple3[0]->texture);
          target->src_blend = targets[i]->tuple3[1]->i;
          target->dst_blend = targets[i]->tuple3[2]->i;
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static gboolean
check_target_types (const CgValue *const *targets,
                    guint n_targets)
{
  for (guint i = 0; i < n_targets; i++)
    {
      if (targets[i]->type == CG_TYPE_TEXTURE
          || (targets[i]->type == CG_TYPE_TUPLE3
              && targets[i]->tuple3[0]->type == CG_TYPE_TEXTURE
              && targets[i]->tuple3[1]->type == CG_TYPE_INT
              && targets[i]->tuple3[1]->i > CG_BLEND_0
              && targets[i]->tuple3[1]->i < CG_N_BLENDS
              && targets[i]->tuple3[2]->type == CG_TYPE_INT
              && targets[i]->tuple3[2]->i > CG_BLEND_0
              && targets[i]->tuple3[2]->i < CG_N_BLENDS))
        continue;
      return FALSE;
    }
  return TRUE;
}

void
cg_plan_config_targets (
    CgPlan *self,
    const CgValue *first_target,
    ...)
{
  guint n_targets = 0;
  const CgValue *targets[32] = { 0 };

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (first_target != NULL);

  CG_PRIV_GATHER_VA_ARGS_INTO (
      first_target, targets, G_N_ELEMENTS (targets), n_targets);

  g_return_if_fail (check_target_types (targets, n_targets));

  config_targets (self, targets, n_targets);
}

void
cg_plan_config_targets_v (
    CgPlan *self,
    const CgValue *const *targets,
    guint n_targets)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (targets != NULL);
  g_return_if_fail (n_targets > 0);
  g_return_if_fail (check_target_types (targets, n_targets));

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
             const CgValue *value)
{
  char *key = NULL;
  CgValue *new_value = NULL;

  key = g_strdup (value->keyval.foreign.key);
  new_value = cg_priv_transfer_value_from_static_foreign (
      CG_PRIV_CREATE (new_value),
      value->keyval.foreign.val);

  g_hash_table_replace (self->configuring->pass.uniforms.hash, key, new_value);
  g_ptr_array_add (self->configuring->pass.uniforms.order, key);
}

static gboolean
check_uniform_types (const CgValue *const *uniforms,
                     guint n_uniforms)
{
  for (guint i = 0; i < n_uniforms; i++)
    {
      if (uniforms[i]->type == CG_TYPE_KEYVAL)
        continue;
      return FALSE;
    }
  return TRUE;
}

void
cg_plan_config_uniforms (
    CgPlan *self,
    const CgValue *first_keyval,
    ...)
{
  const CgValue *keyvals[32] = { 0 };
  guint n_keyvals = 0;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (first_keyval != NULL);

  CG_PRIV_GATHER_VA_ARGS_INTO (
      first_keyval, keyvals, G_N_ELEMENTS (keyvals), n_keyvals);

  g_return_if_fail (check_uniform_types (keyvals, n_keyvals));

  for (guint i = 0; i < n_keyvals; i++)
    add_uniform (self, keyvals[i]);
}

void
cg_plan_config_uniforms_v (
    CgPlan *self,
    const CgValue *const *keyvals,
    guint n_keyvals)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (keyvals != NULL);
  g_return_if_fail (n_keyvals > 0);
  g_return_if_fail (check_uniform_types (keyvals, n_keyvals));

  for (guint i = 0; i < n_keyvals; i++)
    add_uniform (self, keyvals[i]);
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

  dest = (int *)self->configuring->pass.dest.val;

  dest[0] = x;
  dest[1] = y;
  dest[2] = width;
  dest[3] = height;

  self->configuring->pass.dest.set = TRUE;
}

void
cg_plan_config_write_mask (
    CgPlan *self,
    guint32 mask)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  self->configuring->pass.write_mask.val = mask;
  self->configuring->pass.write_mask.set = TRUE;
}

void
cg_plan_config_depth_test_func (
    CgPlan *self,
    int func)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);
  g_return_if_fail (func > CG_TEST_FUNC_0 && func < CG_N_TEST_FUNCS);

  self->configuring->pass.depth_test_func.val = func;
  self->configuring->pass.depth_test_func.set = TRUE;
}

void
cg_plan_config_clockwise_faces (
    CgPlan *self,
    gboolean clockwise)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  self->configuring->pass.clockwise_faces.val = clockwise;
  self->configuring->pass.clockwise_faces.set = TRUE;
}

void
cg_plan_config_backface_cull (
    CgPlan *self,
    gboolean cull)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  self->configuring->pass.backface_cull.val = cull;
  self->configuring->pass.backface_cull.set = TRUE;
}

void
cg_plan_push_group (CgPlan *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring != NULL);

  if (self->cur_instr != NULL)
    {
      CgPrivInstr *parent_pass = self->cur_instr->data;

      self->configuring->pass.fake = TRUE;

      if (self->configuring->pass.targets->len == 0)
        CG_PRIV_REPLACE_POINTER_REF (
            &self->configuring->pass.targets,
            parent_pass->pass.targets,
            g_array_ref, g_array_unref);
      else
        self->configuring->pass.fake = FALSE;

      if (self->configuring->pass.shader == NULL)
        self->configuring->pass.shader = cg_shader_ref (parent_pass->pass.shader);
      else
        self->configuring->pass.fake = FALSE;

      if (self->configuring->pass.fake)
        self->configuring->depth = parent_pass->depth;

      if (!self->configuring->pass.dest.set)
        memcpy (self->configuring->pass.dest.val,
                parent_pass->pass.dest.val,
                sizeof (self->configuring->pass.dest.val));

      if (!self->configuring->pass.write_mask.set)
        self->configuring->pass.write_mask.val = parent_pass->pass.write_mask.val;

      if (!self->configuring->pass.depth_test_func.set)
        self->configuring->pass.depth_test_func.val = parent_pass->pass.depth_test_func.val;

      if (!self->configuring->pass.clockwise_faces.set)
        self->configuring->pass.clockwise_faces.val = parent_pass->pass.clockwise_faces.val;

      if (!self->configuring->pass.backface_cull.set)
        self->configuring->pass.backface_cull.val = parent_pass->pass.backface_cull.val;

      self->cur_instr = g_node_append_data (
          self->cur_instr,
          g_steal_pointer (&self->configuring));
    }
  else
    {
      self->configuring->pass.fake = FALSE;

      if (!self->configuring->pass.write_mask.set)
        {
          self->configuring->pass.write_mask.val = CG_WRITE_MASK_ALL;
          self->configuring->pass.write_mask.set = TRUE;
        }
      if (!self->configuring->pass.depth_test_func.set)
        {
          self->configuring->pass.depth_test_func.val = CG_TEST_LEQUAL;
          self->configuring->pass.depth_test_func.set = TRUE;
        }
      if (!self->configuring->pass.clockwise_faces.set)
        {
          self->configuring->pass.clockwise_faces.val = FALSE;
          self->configuring->pass.clockwise_faces.set = TRUE;
        }
      if (!self->configuring->pass.backface_cull.set)
        {
          self->configuring->pass.backface_cull.val = TRUE;
          self->configuring->pass.backface_cull.set = TRUE;
        }

      CG_PRIV_REPLACE_POINTER (
          &self->root_instr,
          g_node_new (g_steal_pointer (&self->configuring)),
          cg_priv_destroy_instr_node);
      self->cur_instr = self->root_instr;
    }
}

static void
set_dest_from_value (CgPlan *self,
                     const CgValue *value)
{
  g_return_if_fail (value->type == CG_TYPE_RECT);
  cg_plan_config_dest (
      self,
      value->rect[0],
      value->rect[1],
      value->rect[2],
      value->rect[3]);
}

static void
set_write_mask_from_value (CgPlan *self,
                           const CgValue *value)
{
  g_return_if_fail (value->type == CG_TYPE_UINT);
  cg_plan_config_write_mask (self, value->ui);
}

static void
set_depth_test_func_from_value (CgPlan *self,
                                const CgValue *value)
{
  g_return_if_fail (value->type == CG_TYPE_INT);
  cg_plan_config_depth_test_func (self, value->i);
}

static void
set_clockwise_faces_from_value (CgPlan *self,
                                const CgValue *value)
{
  g_return_if_fail (value->type == CG_TYPE_BOOL);
  cg_plan_config_clockwise_faces (self, value->b);
}

static void
set_backface_cull_from_value (CgPlan *self,
                              const CgValue *value)
{
  g_return_if_fail (value->type == CG_TYPE_BOOL);
  cg_plan_config_backface_cull (self, value->b);
}

void
cg_plan_push_state (
    CgPlan *self,
    int first_prop,
    const CgValue *first_value,
    ...)
{
  va_list var_args = { 0 };
  int key = 0;
  const CgValue *value = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (first_value != NULL);

  key = first_prop;
  value = first_value;

  cg_plan_begin_config (self);

  va_start (var_args, first_value);
  for (;;)
    {
      if (key <= CG_STATE_0 || key >= CG_N_STATES)
        {
          CG_PRIV_CRITICAL ("Key %d was not recognized as valid.", key);
          break;
        }

      switch (key)
        {
        case CG_STATE_SHADER:
          cg_plan_config_shader (self, value->shader);
          break;
        case CG_STATE_TARGET:
          cg_plan_config_targets (self, value, NULL);
          break;
        case CG_STATE_UNIFORM:
          cg_plan_config_uniforms (self, value, NULL);
          break;
        case CG_STATE_DEST:
          set_dest_from_value (self, value);
          break;
        case CG_STATE_WRITE_MASK:
          set_write_mask_from_value (self, value);
          break;
        case CG_STATE_DEPTH_FUNC:
          set_depth_test_func_from_value (self, value);
          break;
        case CG_STATE_CLOCKWISE_FACES:
          set_clockwise_faces_from_value (self, value);
          break;
        case CG_STATE_BACKFACE_CULL:
          set_backface_cull_from_value (self, value);
          break;
        default:
          CG_PRIV_CRITICAL ("%d is not a recognized state enum.", key);
          break;
        }

      key = va_arg (var_args, int);
      if (key == 0) break;
      value = va_arg (var_args, const CgValue *);
      if (value == NULL)
        {
          CG_PRIV_CRITICAL ("Trailing keyval pair does not have a value.");
          break;
        }
    }
  va_end (var_args);

  cg_plan_push_group (self);
}

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

      if (!has_shader)
        has_shader = instr->pass.shader != NULL;

      if (!has_depth_func
          && !has_write_mask
          && instr->pass.write_mask.set
          && !(instr->pass.write_mask.val & CG_WRITE_MASK_DEPTH))
        {
          has_write_mask = TRUE;
          has_depth_func = TRUE;
        }
      else
        {
          if (!has_write_mask)
            has_write_mask = instr->pass.write_mask.set;
          if (!has_depth_func)
            has_depth_func = instr->pass.depth_test_func.set;
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

static void
append_buffers (CgPlan *self,
                guint instances,
                CgBuffer **buffers,
                guint n_buffers)
{
  CgPrivInstr *instr = NULL;

  g_assert (self->configuring == NULL);
  g_assert (self->cur_instr != NULL);

  instr = CG_PRIV_CREATE (instr);
  instr->type = CG_PRIV_INSTR_VERTICES;

  if (n_buffers > 1)
    {
      CgBuffer **buffers_dup = NULL;

      buffers_dup = g_malloc0_n (n_buffers, sizeof (*buffers_dup));
      for (guint i = 0; i < n_buffers; i++)
        buffers_dup[i] = cg_buffer_ref (buffers[i]);

      instr->vertices.many_buffers = buffers_dup;
    }
  else
    instr->vertices.one_buffer = cg_buffer_ref (*buffers);

  instr->vertices.n_buffers = n_buffers;
  instr->vertices.instances = instances;

  g_node_append_data (self->cur_instr, instr);
}

void
cg_plan_append (
    CgPlan *self,
    guint instances,
    CgBuffer *first_buffer,
    ...)
{
  CgBuffer *buffers[32] = { 0 };
  guint n_buffers = 0;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);
  g_return_if_fail (instances > 0);
  g_return_if_fail (first_buffer != NULL);
  g_return_if_fail (validate_append (self));

  CG_PRIV_GATHER_VA_ARGS_INTO (
      first_buffer, buffers, G_N_ELEMENTS (buffers), n_buffers);

  append_buffers (self, instances, buffers, n_buffers);
}

void
cg_plan_append_v (
    CgPlan *self,
    guint instances,
    CgBuffer **buffers,
    guint n_buffers)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);
  g_return_if_fail (instances > 0);
  g_return_if_fail (buffers != NULL);
  g_return_if_fail (n_buffers > 0);
  g_return_if_fail (validate_append (self));

  append_buffers (self, instances, buffers, n_buffers);
}

void
cg_plan_blit (CgPlan *self,
              CgTexture *src)
{
  CgPrivInstr *instr = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->configuring == NULL);
  g_return_if_fail (self->cur_instr != NULL);
  g_return_if_fail (src != NULL);

  instr = CG_PRIV_CREATE (instr);
  instr->type = CG_PRIV_INSTR_BLIT;
  instr->blit.src = cg_texture_ref (src);

  g_node_append_data (self->cur_instr, instr);
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
  g_return_val_if_fail (self->cur_instr == NULL, NULL);

  gpu = cg_gpu_ref (self->gpu);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self->gpu, NULL);
  commands = self->gpu->impl->plan_unref_to_commands (self, FALSE, &local_error);
  CG_PRIV_LEAVE (gpu);

  CG_PRIV_HANDLE_BACKEND_ERROR (commands != NULL, error, local_error, gpu, NULL);

  return g_steal_pointer (&commands);
}

CgCommands *
cg_plan_unref_to_debugging_commands (
    CgPlan *self,
    GError **error)
{
  g_autoptr (CgGpu) gpu = NULL;
  g_autoptr (CgCommands) commands = NULL;
  g_autoptr (GError) local_error = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->cur_instr == NULL, NULL);

  gpu = cg_gpu_ref (self->gpu);

  CG_PRIV_TRY_ENTER_ORELSE_RETURN_VAL (self->gpu, NULL);
  commands = self->gpu->impl->plan_unref_to_commands (self, TRUE, &local_error);
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

GPtrArray *
cg_commands_ref_last_debug_dispatch (CgCommands *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->debug.enabled, NULL);
  g_return_val_if_fail (self->debug.calls.run != NULL, NULL);

  return g_ptr_array_ref (self->debug.calls.run);
}
