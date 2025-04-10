/* cpc-gpu-util.c
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

#include "cpc-gpu-private.h"

static const char *type_names[CG_N_TYPES] = {
#define ADD_TYPE(name) [CG_TYPE_##name] = G_STRINGIFY (name)
  ADD_TYPE (SHADER),
  ADD_TYPE (BUFFER),
  ADD_TYPE (TEXTURE),
  ADD_TYPE (BOOL),
  ADD_TYPE (INT),
  ADD_TYPE (UINT),
  ADD_TYPE (FLOAT),
  ADD_TYPE (POINTER),
  ADD_TYPE (VEC2),
  ADD_TYPE (VEC3),
  ADD_TYPE (VEC4),
  ADD_TYPE (MAT4),
  ADD_TYPE (RECT),
  ADD_TYPE (KEYVAL),
  ADD_TYPE (TUPLE2),
  ADD_TYPE (TUPLE3),
  ADD_TYPE (TUPLE4),
#undef ADD_TYPE
};

const char *
cg_priv_get_type_name (int type)
{
  g_assert (type > CG_TYPE_0 && type < CG_N_TYPES);
  return type_names[type];
}

CgValue *
cg_priv_transfer_value_from_static_foreign (
    CgValue *dest,
    const CgValue *src)
{
  if (src->type == CG_TYPE_SHADER)
    {
      dest->type = CG_TYPE_SHADER;
      dest->shader = cg_shader_ref (src->shader);
    }
  else if (src->type == CG_TYPE_TEXTURE)
    {
      dest->type = CG_TYPE_TEXTURE;
      dest->texture = cg_texture_ref (src->texture);
    }
  else if (src->type == CG_TYPE_BUFFER)
    {
      dest->type = CG_TYPE_BUFFER;
      dest->buffer = cg_buffer_ref (src->buffer);
    }
  else if (src->type == CG_TYPE_MAT4 && src->mat4.foreign != NULL)
    {
      dest->type = CG_TYPE_MAT4;
      dest->mat4.initialized = g_memdup2 (src->mat4.foreign, 16 * sizeof (float));
    }
  else if (src->type == CG_TYPE_KEYVAL && src->keyval.foreign.key != NULL)
    {
      dest->type = CG_TYPE_KEYVAL;
      dest->keyval.initialized.key = g_strdup (src->keyval.foreign.key);
      dest->keyval.initialized.val = CG_PRIV_CREATE (dest->keyval.initialized.val);
      cg_priv_transfer_value_from_static_foreign (dest->keyval.initialized.val, src->keyval.foreign.val);
    }
  else
    *dest = *src;

  return dest;
}

void
cg_priv_clear_value (CgValue *self)
{
  switch (self->type)
    {
    case CG_TYPE_SHADER:
      g_clear_pointer (&self->shader, cg_shader_unref);
      break;
    case CG_TYPE_BUFFER:
      g_clear_pointer (&self->buffer, cg_buffer_unref);
      break;
    case CG_TYPE_TEXTURE:
      g_clear_pointer (&self->texture, cg_texture_unref);
      break;
    case CG_TYPE_MAT4:
      g_clear_pointer (&self->mat4.initialized, g_free);
      break;
    case CG_TYPE_KEYVAL:
      g_clear_pointer (&self->keyval.initialized.key, g_free);
      g_clear_pointer (&self->keyval.initialized.val, cg_priv_destroy_value);
      break;
    default:
      break;
    }
}

void
cg_priv_destroy_value (gpointer data)
{
  CgValue *value = data;

  cg_priv_clear_value (value);
  g_free (value);
}

void
cg_priv_clear_target (gpointer data)
{
  CgPrivTarget *target = data;

  g_clear_pointer (&target->texture, cg_texture_unref);
}

void
cg_priv_clear_data_layout (CgDataSegment *layout,
                           guint length)
{
  if (layout == NULL)
    return;

  for (guint i = 0; i < length; i++)
    g_free (layout[i].name);
}
