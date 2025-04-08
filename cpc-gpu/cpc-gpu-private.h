/* cpc-gpu-private.h
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

#pragma once

#include <cpc-gpu.h>

#include "config.h"

G_BEGIN_DECLS

#define CG_PRIV_CRITICAL(fmt, ...) g_critical ("In function %s: " fmt, G_STRFUNC, ##__VA_ARGS__)
#define CG_PRIV_FATAL(...)                                        \
  G_STMT_START                                                    \
  {                                                               \
    CG_PRIV_CRITICAL ("A FATAL ERROR HAS OCCURED: " __VA_ARGS__); \
    g_assert_not_reached ();                                      \
  }                                                               \
  G_STMT_END

#define CG_PRIV_REPLACE_POINTER(pp, np, unref) \
  G_STMT_START                                 \
  {                                            \
    g_clear_pointer (pp, unref);               \
    *(pp) = np;                                \
  }                                            \
  G_STMT_END

#define CG_PRIV_REPLACE_POINTER_REF(pp, np, ref, unref) \
  G_STMT_START                                          \
  {                                                     \
    g_clear_pointer (pp, unref);                        \
    *(pp) = ref (np);                                   \
  }                                                     \
  G_STMT_END

#define CG_PRIV_CREATE(var) g_new0 (typeof (*var), 1)

typedef struct
{
  gboolean is_threadsafe;
  CgGpu *(*get_gpu_for_this_thread) (void);
  void (*set_gpu_for_this_thread) (CgGpu *gpu);

  CgGpu *(*gpu_new) (
      guint32 flags,
      gpointer extra_data,
      GError **error);
  CgGpu *(*gpu_ref) (CgGpu *self);
  void (*gpu_unref) (CgGpu *self);
  char *(*gpu_get_info) (
      CgGpu *self,
      const char *param,
      GError **error);
  gboolean (*gpu_flush) (
      CgGpu *self,
      GError **error);

#define DECLARE_ANCILLARY_OBJECT(lower, upper)  \
  Cg##upper *(*lower##_new) (CgGpu * gpu);      \
  Cg##upper *(*lower##_ref) (Cg##upper * self); \
  void (*lower##_unref) (Cg##upper * self);

  DECLARE_ANCILLARY_OBJECT (plan, Plan)
  DECLARE_ANCILLARY_OBJECT (shader, Shader)
  DECLARE_ANCILLARY_OBJECT (buffer, Buffer)
  DECLARE_ANCILLARY_OBJECT (texture, Texture)
  DECLARE_ANCILLARY_OBJECT (commands, Commands)

#undef DECLARE_ANCILLARY_OBJECT

  CgCommands *(*plan_unref_to_commands) (
      CgPlan *self,
      GError **error);

  gboolean (*commands_dispatch) (
      CgCommands *self,
      GError **error);

} CgBackendImpl;

struct _CgGpu
{
  gint32 atomic_field;
  gboolean threadsafe;

  gboolean debug_output;
  gboolean exit_on_error;

  const CgBackendImpl *impl;
};
void cg_priv_finish (CgGpu *self);

#define CG_PRIV_DEAL_WITH_THREADS(gpu) (!(gpu)->impl->is_threadsafe && (gpu)->threadsafe)
#define CG_PRIV_ENTER_BIT(gpu, bit)             \
  G_STMT_START                                  \
  {                                             \
    if (CG_PRIV_DEAL_WITH_THREADS (gpu))        \
      g_bit_lock (&(gpu)->atomic_field, (bit)); \
  }                                             \
  G_STMT_END
#define CG_PRIV_LEAVE_BIT(gpu, bit)               \
  G_STMT_START                                    \
  {                                               \
    if (CG_PRIV_DEAL_WITH_THREADS (gpu))          \
      g_bit_unlock (&(gpu)->atomic_field, (bit)); \
  }                                               \
  G_STMT_END
#define CG_PRIV_DATA_LOCK_BIT 30

enum
{
  CG_PRIV_INSTR_PASS = 0,
  CG_PRIV_INSTR_VERTICES,
  CG_PRIV_INSTR_BLIT,
};

typedef struct
{
  guint depth;
  int type;

  union
  {
    struct
    {
      CgShader *shader;
      GPtrArray *targets;
      GHashTable *attributes;
      struct
      {
        GHashTable *hash;
        GPtrArray *order;
      } uniforms;
      int dest[4];
      guint32 write_mask;
      int depth_test_func;
    } pass;

    struct
    {
      guint n_buffers;
      union
      {
        CgBuffer *one_buffer;
        CgBuffer **many_buffers;
      };
      guint instances;
    } vertices;

    struct
    {
      CgTexture *src;
    } blit;
  };

  gpointer user_data;
  GDestroyNotify destroy_user_data;

} CgPrivInstr;
void cg_priv_destroy_instr_node (GNode *self);

struct _CgPlan
{
  CgGpu *gpu;

  GNode *root_instr;
  GNode *cur_instr;

  CgPrivInstr *configuring;
};
void cg_priv_plan_finish (CgPlan *self);

struct _CgShader
{
  CgGpu *gpu;

  struct
  {
    char *vertex_code;
    char *fragment_code;
  } init;
};
void cg_priv_shader_finish (CgShader *self);

struct _CgBuffer
{
  CgGpu *gpu;

  CgDataSegment *spec;
  guint spec_length;

  struct
  {
    gpointer data;
    gsize size;
  } init;
};
void cg_priv_buffer_finish (CgBuffer *self);

#define CG_PRIV_FORMAT_DEPTH -1
struct _CgTexture
{
  CgGpu *gpu;

  struct
  {
    gboolean cubemap;
    gpointer data;
    int width;
    int height;
    int format;
    int mipmaps;
    int msaa;
  } init;
};
void cg_priv_texture_finish (CgTexture *self);

struct _CgCommands
{
  CgGpu *gpu;

  char *debug_output;
};
CgCommands *cg_priv_commands_new (CgGpu *gpu);
void cg_priv_commands_finish (CgCommands *self);

extern const CgBackendImpl cg_gl_impl;
extern const CgBackendImpl cg_vk_impl;

const char *cg_priv_get_type_name (int type);
CgValue *cg_priv_transfer_value_from_static_foreign (
    CgValue *dest,
    const CgValue *src);
void cg_priv_clear_value (CgValue *self);
void cg_priv_destroy_value (gpointer data);

static inline void
clear_data_layout (CgDataSegment *layout,
                   guint length)
{
  if (layout == NULL)
    return;

  for (guint i = 0; i < length; i++)
    g_free (layout[i].name);
}

G_END_DECLS
