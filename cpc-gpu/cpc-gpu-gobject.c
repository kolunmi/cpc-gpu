/* cpc-gpu-gobject.c
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

#include "cpc-gpu-gobject.h"

G_DEFINE_BOXED_TYPE (CgGpu, cpc_gpu, cg_gpu_ref, cg_gpu_unref);
G_DEFINE_BOXED_TYPE (CgShader, cg_shader, cg_shader_ref, cg_shader_unref);
G_DEFINE_BOXED_TYPE (CgBuffer, cg_buffer, cg_buffer_ref, cg_buffer_unref);
G_DEFINE_BOXED_TYPE (CgTexture, cg_texture, cg_texture_ref, cg_texture_unref);
G_DEFINE_BOXED_TYPE (CgPlan, cg_plan, cg_plan_ref, cg_plan_unref);
G_DEFINE_BOXED_TYPE (CgCommands, cg_commands, cg_commands_ref, cg_commands_unref);
