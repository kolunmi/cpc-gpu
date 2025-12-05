/* cpc-gpu-gobject.h
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
#include <glib-object.h>

G_BEGIN_DECLS

#define CPC_TYPE_GPU cpc_gpu_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_gpu_get_type (void) G_GNUC_CONST;

#define CPC_TYPE_GPU_SHADER cpc_gpu_shader_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_shader_get_type (void) G_GNUC_CONST;

#define CPC_TYPE_GPU_BUFFER cpc_gpu_buffer_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_buffer_get_type (void) G_GNUC_CONST;

#define CPC_TYPE_GPU_TEXTURE cpc_gpu_texture_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_texture_get_type (void) G_GNUC_CONST;

#define CPC_TYPE_GPU_PLAN cpc_gpu_plan_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_plan_get_type (void) G_GNUC_CONST;

#define CPC_TYPE_GPU_COMMANDS cpc_gpu_commands_get_type ()
CPC_GPU_AVAILABLE_IN_ALL
GType cg_commands_get_type (void) G_GNUC_CONST;

G_END_DECLS
