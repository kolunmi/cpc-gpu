/* cpc-gpu-version-macros.h
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

#include <glib.h>

#include "cpc-gpu-version.h"

#ifndef _CPC_GPU_EXTERN
#define _CPC_GPU_EXTERN extern
#endif

#define CPC_GPU_VERSION_CUR_STABLE (G_ENCODE_VERSION (CPC_GPU_MAJOR_VERSION, 0))

#ifdef CPC_GPU_DISABLE_DEPRECATION_WARNINGS
#define CPC_GPU_DEPRECATED _CPC_GPU_EXTERN
#define CPC_GPU_DEPRECATED_FOR(f) _CPC_GPU_EXTERN
#define CPC_GPU_UNAVAILABLE(maj, min) _CPC_GPU_EXTERN
#else
#define CPC_GPU_DEPRECATED G_DEPRECATED _CPC_GPU_EXTERN
#define CPC_GPU_DEPRECATED_FOR(f) G_DEPRECATED_FOR (f) _CPC_GPU_EXTERN
#define CPC_GPU_UNAVAILABLE(maj, min) G_UNAVAILABLE (maj, min) _CPC_GPU_EXTERN
#endif

#define CPC_GPU_VERSION_1_0 (G_ENCODE_VERSION (1, 0))

#if CPC_GPU_MAJOR_VERSION == CPC_GPU_VERSION_1_0
#define CPC_GPU_VERSION_PREV_STABLE (CPC_GPU_VERSION_1_0)
#else
#define CPC_GPU_VERSION_PREV_STABLE (G_ENCODE_VERSION (CPC_GPU_MAJOR_VERSION - 1, 0))
#endif

#ifndef CPC_GPU_VERSION_MIN_REQUIRED
#define CPC_GPU_VERSION_MIN_REQUIRED (CPC_GPU_VERSION_CUR_STABLE)
#endif

#ifndef CPC_GPU_VERSION_MAX_ALLOWED
#if CPC_GPU_VERSION_MIN_REQUIRED > CPC_GPU_VERSION_PREV_STABLE
#define CPC_GPU_VERSION_MAX_ALLOWED (CPC_GPU_VERSION_MIN_REQUIRED)
#else
#define CPC_GPU_VERSION_MAX_ALLOWED (CPC_GPU_VERSION_CUR_STABLE)
#endif
#endif

#define CPC_GPU_AVAILABLE_IN_ALL _CPC_GPU_EXTERN

#if CPC_GPU_VERSION_MIN_REQUIRED >= CPC_GPU_VERSION_1_0
#define CPC_GPU_DEPRECATED_IN_1_0 CPC_GPU_DEPRECATED
#define CPC_GPU_DEPRECATED_IN_1_0_FOR(f) CPC_GPU_DEPRECATED_FOR (f)
#else
#define CPC_GPU_DEPRECATED_IN_1_0 _CPC_GPU_EXTERN
#define CPC_GPU_DEPRECATED_IN_1_0_FOR(f) _CPC_GPU_EXTERN
#endif
#if CPC_GPU_VERSION_MAX_ALLOWED < CPC_GPU_VERSION_1_0
#define CPC_GPU_AVAILABLE_IN_1_0 CPC_GPU_UNAVAILABLE (1, 0)
#else
#define CPC_GPU_AVAILABLE_IN_1_0 _CPC_GPU_EXTERN
#endif
