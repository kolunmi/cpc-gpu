/* cpc-gpu.h
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

/*! CpcGpu by Adam Masciola
 *
 * @brief Rendering Abstraction Library for Carapace.
 *
 * # CpcGpu
 *
 * This library abstracts the usage of a graphics API. All
 * supported backends are handled internally, and the specific
 * backend utilized is only ever specified once upon initialization
 * of a master object of type @a CgGpu . The currently supported
 * backends are OpenGL and one day Vulkan.
 *
 * Note that this library is not responsible for the creation of
 * the graphics context. This is left up to the user, meaning that,
 * for applicable backends, the user must notify CpcGpu when the
 * context switches. This can be achieved with
 * @a cg_gpu_steal_this_thread .
 *
 * This library utilizes [GLib](https://gitlab.gnome.org/GNOME/glib).
 * I will outline some of the practices this project inherits from
 * GLib here:
 *
 * ## Basic Usage Patterns
 *
 * ### Refcounting
 *
 * CpcGpu uses atomic reference counting to manage the lifetime of
 * resources. Each object declared in this header has corresponding
 * "ref" and "unref" functions. The "ref" function increments the
 * reference count, and the "unref" function decrements it. If you
 * release the last reference to an object, or in other words call
 * "unref" when the reference count is 1, the object is destroyed.
 *
 * When you want to maintain access to an object even when leaving
 * a scope, you need to call the "ref" function to create a strong
 * reference. When you no longer need it, call "unref". Once you've
 * done this, the object is no longer valid, so I recommend using
 * the `g_clear_pointer()` macro to call the "unref" function and
 * zero out the location of the strong reference:
 *
 * ```c
 * CgBuffer *location = cg_buffer_ref (buffer);
 * // ...
 * g_clear_pointer (&location, cg_buffer_unref);
 * ```
 *
 * ### Error Handling
 *
 * GLib provides a structure for error handling called `GError`.
 * When you call a function from CpcGpu and an error occurs inside
 * that function _which is not the direct result of programmer error_
 * from which you would like to recover and obtain details rather than
 * take some agnostic action, you will need to use this structure.
 * Functions which may error out will always have a parameter of type
 * `GError **error` as the final parameter. If you don't care about
 * receiving the error, pass `NULL`. Otherwise you must pass the
 * address of a variable initialized to `NULL`. For example:
 *
 * ```c
 * GError *error = NULL;
 * gboolean success = FALSE;
 *
 * success = cg_commands_dispatch (commands, &error);
 *
 * if (!success)
 *   {
 *     g_critical ("An error occured: %s", error->message);
 *     // ...
 *     g_error_free (error);
 *   }
 * ```
 *
 * For more information on using `GError`, see the
 * [official docs](https://docs.gtk.org/glib/error-reporting.html).
 *
 * If the programmer made a mistake and CpcGpu cannot continue, the
 * function will not populate the error and instead log a critical
 * warning. An example of a programmer error would be passing a `NULL`
 * value as a parameter or otherwise using the API incorrectly.
 *
 */

#pragma once

#define CPC_GPU_INSIDE

#include <glib.h>

#include "cpc-gpu-version-macros.h"

G_BEGIN_DECLS

/*! @brief The main GPU `GError` domain. */
#define CG_ERROR (cg_error_quark ())
GQuark cg_error_quark (void);

/*! @brief Error codes for the `GError`
 *         domain @a CG_ERROR.
 *
 * For use with `GError`. See the error's
 * message field for detailed information
 * of the specific issue.
 *
 */
typedef enum
{
  CG_ERROR_FAILED_INIT = 0,           /*!< Could not initialize the main GPU object,
                                           usually due to failed extension loading. */
  CG_ERROR_FAILED_SHADER_GEN,         /*!< Could not generate a shader, usually due
                                           to compilation or linking issues. */
  CG_ERROR_FAILED_SHADER_UNIFORM_SET, /*!< Could not set a uniform, usually due to
                                           the uniform not existing or type mismatches. */
  CG_ERROR_FAILED_BUFFER_GEN,         /*!< Could not generate a buffer of some type. */
  CG_ERROR_FAILED_TEXTURE_GEN,        /*!< Could not generate a texture. */
  CG_ERROR_FAILED_TARGET_CREATION,    /*!< Could not create a target object due to the
                                           failed generation of an underlying object or
                                           the underlying framebuffer ultimately being
                                           incomplete. */
  CG_N_ERRORS
} CgError;

/*! @class CgGpu
 *
 * @brief The main GPU abstraction object.
 *
 * The main object through which a GPU API is
 * accessible. All other objects created through
 * this object will maintain a strong reference
 * to it.
 *
 */
typedef struct _CgGpu CgGpu;

/*! @class CgShader
 *
 * @brief A shader resource.
 *
 * An object representing user defined code
 * which transforms data on the GPU.
 *
 */
typedef struct _CgShader CgShader;

/*! @class CgBuffer
 *
 * @brief A generic buffer resource.
 *
 * An object representing data uploaded to the
 * GPU. Internal operations on the buffer start out
 * as generic as possible, then, depending on how
 * it is used, the backend may assign it certain
 * properties which could render it incompatible
 * in other situations. For instance, with the
 * OpenGL backend, if you initially use it as a
 * uniform buffer, you may not be able to later
 * use it as vertex data.
 *
 */
typedef struct _CgBuffer CgBuffer;

/*! @class CgTexture
 *
 * @brief A texture resource.
 *
 * An object representing an image on the GPU.
 * Note that many properties of the image, such
 * as the dimensions, are immutable.
 *
 */
typedef struct _CgTexture CgTexture;

/*! @class CgPlan
 *
 * @brief An outline of operations intended to
 *        be compiled and ran by the backend.
 *
 * Since this object is just an outline, the backend
 * is never invoked through it until you call
 * @a cg_plan_unref_to_commands at which point
 * it is processed and destroyed. This means that the
 * construction of this object can be done in any thread.
 *
 */
typedef struct _CgPlan CgPlan;

/*! @class CgCommands
 *
 * @brief An object containing backend specific
 *        information and instructions that may
 *        be invoked under the correct
 *        circumstances.
 *
 * To obtain an instance this object, store the
 * result of @a cg_plan_unref_to_commands
 *
 */
typedef struct _CgCommands CgCommands;

/*! @brief A component of a contiguous data
 *         layout for an @a CgBuffer
 *
 */
typedef struct
{
  char *name;        /*!< The attribute name. */
  int type;          /*!< The data type. */
  int num;           /*!< The length of this segment. */
  int instance_rate; /*!< The rate at which the segment is applied
                          per instanced render. 0 indicates that
                          the segment will be applied once for
                          every element. */
} CgDataSegment;

/*! @brief Initialization flags.
 *
 * For use with @a cg_gpu_new
 *
 */
enum
{
  CG_INIT_FLAG_BACKEND_OPENGL = 1 << 0,   /*!< https://www.opengl.org/ */
  CG_INIT_FLAG_BACKEND_VULKAN = 1 << 1,   /*!< https://www.vulkan.org/ */
  CG_INIT_FLAG_USE_DEBUG_LAYERS = 1 << 2, /*!< Output backed-specific debug information. */
  CG_INIT_FLAG_NO_THREAD_SAFETY = 1 << 3, /*!< Always pass checks regarding thread
                                               synchronization, even if doing so will
                                               cause an error. */
  CG_INIT_FLAG_NO_FALLBACK = 1 << 4,      /*!< Do not fall back on another backend if the
                                               requested backend could not be initialized. */
  CG_INIT_FLAG_EXIT_ON_ERROR = 1 << 5,    /*!< Terminate the application if any error
                                               occurs instead of returning errors. */
  CG_INIT_FLAG_LOG_ERRORS = 1 << 6,       /*!< Log all errors returned by functions. */
};

/*! @brief Render pass write flags.
 *
 * Used to enable or disable certain
 * output components.
 *
 */
enum
{
  CG_WRITE_MASK_COLOR_RED = 1 << 0,   /*!< Red */
  CG_WRITE_MASK_COLOR_GREEN = 1 << 1, /*!< Green */
  CG_WRITE_MASK_COLOR_BLUE = 1 << 2,  /*!< Blue */
  CG_WRITE_MASK_COLOR_ALPHA = 1 << 3, /*!< Alpha Transparency */
  CG_WRITE_MASK_DEPTH = 1 << 4,       /*!< Depth Component */

  CG_WRITE_MASK_RGB = CG_WRITE_MASK_COLOR_RED | CG_WRITE_MASK_COLOR_GREEN | CG_WRITE_MASK_COLOR_BLUE, /*!< Just rgb, no alpha or depth */
  CG_WRITE_MASK_COLOR = CG_WRITE_MASK_RGB | CG_WRITE_MASK_COLOR_ALPHA,                                /*!< Just color, no depth */
  CG_WRITE_MASK_ALL = CG_WRITE_MASK_COLOR | CG_WRITE_MASK_DEPTH,                                      /*!< All Components */
};

/*! @brief Basic numerical test functions.
 *
 * Used to control the output in scenerios
 * where data must either be included or
 * rejected.
 *
 */
enum
{
  CG_TEST_FUNC_0 = 0, /*!< DO NOT USE */

  CG_TEST_NEVER,     /*!< `FALSE` */
  CG_TEST_ALWAYS,    /*!< `TRUE` */
  CG_TEST_LESS,      /*!< `x < y` */
  CG_TEST_LEQUAL,    /*!< `x <= y` */
  CG_TEST_GREATER,   /*!< `x > y` */
  CG_TEST_GEQUAL,    /*!< `x >= y` */
  CG_TEST_EQUAL,     /*!< `x == y` */
  CG_TEST_NOT_EQUAL, /*!< `x != y` */

  CG_N_TEST_FUNCS /*!< DO NOT USE */
};

/*! @brief Blending modes
 *
 * Used to control the manner in which component
 * writes merge with existing values.
 *
 */
enum
{
  CG_BLEND_0 = 0, /*!< DO NOT USE */

  CG_BLEND_ZERO,
  CG_BLEND_ONE,
  CG_BLEND_SRC_COLOR,
  CG_BLEND_ONE_MINUS_SRC_COLOR,
  CG_BLEND_DST_COLOR,
  CG_BLEND_ONE_MINUS_DST_COLOR,
  CG_BLEND_SRC_ALPHA,
  CG_BLEND_ONE_MINUS_SRC_ALPHA,
  CG_BLEND_DST_ALPHA,
  CG_BLEND_ONE_MINUS_DST_ALPHA,
  CG_BLEND_CONSTANT_COLOR,
  CG_BLEND_ONE_MINUS_CONSTANT_COLOR,
  CG_BLEND_CONSTANT_ALPHA,
  CG_BLEND_ONE_MINUS_CONSTANT_ALPHA,
  CG_BLEND_SRC_ALPHA_SATURATE,
  CG_BLEND_SRC1_COLOR,
  CG_BLEND_ONE_MINUS_SRC1_COLOR,
  CG_BLEND_SRC1_ALPHA,
  CG_BLEND_ONE_MINUS_SRC1_ALPHA,

  CG_N_BLENDS /*!< DO NOT USE */
};

/*! @brief State properties for @a cg_plan_push_state */
enum
{
  CG_STATE_0 = 0, /*!< DO NOT USE */

  CG_STATE_TARGET,          /*!< Add a render target;
                                 | of type: @a CG_TYPE_TEXTURE
                                 | or type: @a CG_TYPE_TUPLE3 { @a CG_TYPE_TEXTURE ,
                                 |                              @a CG_TYPE_INT (src blend) ,
                                 |                              @a CG_TYPE_INT (dest blend) } */
  CG_STATE_SHADER,          /*!< Set the shader;
                                 | of type: @a CG_TYPE_SHADER */
  CG_STATE_UNIFORM,         /*!< Set a uniform;
                                 | of type: @a CG_TYPE_KEYVAL */
  CG_STATE_DEST,            /*!< Set the viewport;
                                 | of type: @a CG_TYPE_RECT */
  CG_STATE_WRITE_MASK,      /*!< Set the write mask;
                                 | of type: @a CG_TYPE_UINT */
  CG_STATE_DEPTH_FUNC,      /*!< Set the depth comparison func;
                                 | of type: @a CG_TYPE_INT */
  CG_STATE_CLOCKWISE_FACES, /*!< If TRUE, triangle front-faces will be determined
                                 using clockwise winding instead of counter-clockwise;
                                 | of type: @a CG_TYPE_BOOL */
  CG_STATE_BACKFACE_CULL,   /*!< Set whether to cull backfaces faces;
                                 | of type: @a CG_TYPE_BOOL */

  CG_N_STATES /*!< DO NOT USE */
};

/*! @brief A @a CgValue type. */
enum
{
  CG_TYPE_0 = 0, /*!< DO NOT USE */

  CG_TYPE_SHADER,  /*!< @a CgShader */
  CG_TYPE_BUFFER,  /*!< @a CgBuffer */
  CG_TYPE_TEXTURE, /*!< @a CgTexture */

  CG_TYPE_BOOL,    /*!< `gboolean` */
  CG_TYPE_INT,     /*!< `int` */
  CG_TYPE_UINT,    /*!< `guint` */
  CG_TYPE_FLOAT,   /*!< `float` */
  CG_TYPE_POINTER, /*!< `gpointer` */

  CG_TYPE_VEC2, /*!< `float[2]` */
  CG_TYPE_VEC3, /*!< `float[3]` */
  CG_TYPE_VEC4, /*!< `float[4]` */
  CG_TYPE_MAT4, /*!< `float[16]` */
  CG_TYPE_RECT, /*!< `int[4]` */

  CG_TYPE_KEYVAL, /*!< `char *` and another @a CgValue */

  CG_TYPE_TUPLE2, /*!< ordered tuple of 2 @a CgValue */
  CG_TYPE_TUPLE3, /*!< ordered tuple of 3 @a CgValue */
  CG_TYPE_TUPLE4, /*!< ordered tuple of 4 @a CgValue */

  CG_N_TYPES /*!< DO NOT USE */
};

/*! @brief A generic value union.
 *
 * Use the associated macros with the form `CG_TYPE ()`
 * to construct this structure as a parameter for a CgGpu
 * function.
 *
 */
typedef struct _CgValue CgValue;

struct _CgValue
{
  int type;

  union
  {
    CgShader *shader;
    CgBuffer *buffer;
    CgTexture *texture;

    gboolean b;
    int i;
    guint ui;
    float f;
    gpointer p;

    float vec2[2];
    float vec3[3];
    float vec4[4];
    int rect[4];

    union
    {
      const float *foreign;
      /* PRIVATE */
      float *initialized;
    } mat4;

    union
    {
      struct
      {
        const char *key;
        const CgValue *val;
      } foreign;
      /* PRIVATE */
      struct
      {
        char *key;
        CgValue *val;
      } initialized;
    } keyval;

    const CgValue *tuple2[2];
    const CgValue *tuple3[3];
    const CgValue *tuple4[4];
  };
};

#define CG_CONVERT_TO_VALUE(t, m, ...) (&(CgValue){ .type = (t), .m = __VA_ARGS__ })

#define CG_SHADER(v) CG_CONVERT_TO_VALUE (CG_TYPE_SHADER, shader, (v))
#define CG_BUFFER(v) CG_CONVERT_TO_VALUE (CG_TYPE_BUFFER, buffer, (v))
#define CG_TEXTURE(v) CG_CONVERT_TO_VALUE (CG_TYPE_TEXTURE, texture, (v))

#define CG_BOOL(v) CG_CONVERT_TO_VALUE (CG_TYPE_BOOL, b, (v))
#define CG_INT(v) CG_CONVERT_TO_VALUE (CG_TYPE_INT, i, (v))
#define CG_UINT(v) CG_CONVERT_TO_VALUE (CG_TYPE_UINT, ui, (v))
#define CG_FLOAT(v) CG_CONVERT_TO_VALUE (CG_TYPE_FLOAT, f, (v))
#define CG_POINTER(v) CG_CONVERT_TO_VALUE (CG_TYPE_POINTER, p, (v))

#define CG_VEC(t, m, ...) CG_CONVERT_TO_VALUE (t, m, { __VA_ARGS__ })
#define CG_VEC2(x, y) CG_VEC (CG_TYPE_VEC2, vec2, x, y)
#define CG_VEC3(x, y, z) CG_VEC (CG_TYPE_VEC3, vec3, x, y, z)
#define CG_VEC4(x, y, z, w) CG_VEC (CG_TYPE_VEC4, vec4, x, y, z, w)
#define CG_RECT(x, y, w, h) CG_VEC (CG_TYPE_RECT, rect, x, y, w, h)
#define CG_MAT4(v) CG_CONVERT_TO_VALUE (CG_TYPE_MAT4, mat4, { .foreign = (v) })

#define CG_KEYVAL(k, v) CG_CONVERT_TO_VALUE (CG_TYPE_KEYVAL, keyval, { .foreign = { .key = (k), .val = (v) } })

#define CG_TUPLE2(one, two) CG_VEC (CG_TYPE_TUPLE2, tuple2, one, two)
#define CG_TUPLE3(one, two, three) CG_VEC (CG_TYPE_TUPLE3, tuple3, one, two, three)
#define CG_TUPLE4(one, two, three, four) CG_VEC (CG_TYPE_TUPLE4, tuple4, one, two, three, four)

/*! @brief A pixel buffer format. */
enum
{
  CG_FORMAT_0 = 0, /*!< DO NOT USE */

  CG_FORMAT_R8,     /*!< grayscale 8-bit */
  CG_FORMAT_RA8,    /*!< 2-channel 8-bit */
  CG_FORMAT_RGB8,   /*!< 3-channel 8-bit */
  CG_FORMAT_RGBA8,  /*!< 4-channel 8-bit */
  CG_FORMAT_R32,    /*!< grayscale float */
  CG_FORMAT_RGB32,  /*!< 3-channel float */
  CG_FORMAT_RGBA32, /*!< 4-channel float */

  CG_N_FORMATS, /*!< DO NOT USE */
};

/*! @brief Create a new @a CgGpu object.
 *
 * @param [in] flags Initialization flags.
 * @param [in] extra_data A pointer to
 *        backend specific initialization
 *        data, such as an extensions loader.
 * @param [out] error The return location
 *        for a recoverable error.
 *
 * @return The newly allocated object.
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgGpu *cg_gpu_new (
    guint32 flags,
    gpointer extra_data,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgGpu object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgGpu *cg_gpu_ref (CgGpu *self);

/*! @brief Release a strong reference
 *         from a @a CgGpu object.
 *
 * @param [in] self The object.
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_gpu_unref (gpointer self);

/*! @brief Get backend specific information
 *         through a string key.
 *
 * @param [in] self The GPU object.
 * @param [in] param The key.
 *
 * Use this function to retrieve version
 * information, max texture size, etc.
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
char *cg_gpu_get_info (
    CgGpu *self,
    const char *param,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief For applicable backends, make
 *         this GPU current to the thread.
 *
 * @param [in] self The GPU object.
 *
 * @return Whether the thread was
 *         successfully obtained.
 *
 * For backends for which this function does not
 * make sense, this function always returns `TRUE`.
 *
 * This function informs the backend that you manually
 * made the context current in this thread using the
 * system with which you instantiated the context, so
 * that threading checks can be made. Always pair this
 * call with that action, such as a call to `eglMakeCurrent`.
 * If you would like to disable thread checking, pass
 * @a CG_INIT_FLAG_NO_THREAD_SAFETY to @a cg_gpu_new
 *
 * If this function succeeds, a ref is taken on the
 * gpu object, and you must later call
 * @a cg_gpu_release_this_thread to disassociate
 * the gpu from the thread and release the ref.
 *
 * See @a cg_gpu_release_this_thread
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
gboolean cg_gpu_steal_this_thread (CgGpu *self);

/*! @brief For applicable backends, command this
 *         GPU to release the current thread.
 *
 * @param [in] self The GPU object.
 *
 * For backends for which this function does not
 * make sense, this function does nothing.
 *
 * See @a cg_gpu_steal_this_thread
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_gpu_release_this_thread (CgGpu *self);

/*! @brief Ensure the GPU context is up to date.
 *
 * @param [in] self The GPU object.
 * @param [out] error The return location
 *        for a recoverable error.
 *
 * @return Whether the operation was successful.
 *
 * This function flushes the context. Usually this is
 * not necessary to call manually, except to immediately
 * release resources, such as a @a CgShader that was
 * recently destroyed.
 *
 * @memberof CgGpu
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
gboolean cg_gpu_flush (CgGpu *self, GError **error);

/*! @brief Create a new @a CgShader object
 *         in accordance with vertex and fragment
 *         shader code.
 *
 * @param [in] self The GPU object.
 * @param [in] vertex_code A zero terminated vertex shader string.
 * @param [in] fragment_code A zero terminated fragment shader string.
 *
 * @return The newly allocated object.
 *
 * @memberof CgShader
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgShader *cg_shader_new_for_code (
    CgGpu *self,
    const char *vertex_code,
    const char *fragment_code) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgShader object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgShader
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgShader *cg_shader_ref (CgShader *self);

/*! @brief Release a strong reference
 *         from a @a CgShader object.
 *
 * @param [in] self The object.
 *
 * @memberof CgShader
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_shader_unref (gpointer self);

/*! @brief Create a new @a CgBuffer object
 *         with initial duplicated data.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] spec The data layout spec to hint.
 * @param [in] spec_length The length of the layout spec buffer.
 *
 * @return The newly allocated object.
 *
 * @memberof CgBuffer
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgBuffer *cg_buffer_new_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    const CgDataSegment *spec,
    guint spec_length) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Like @a cg_buffer_new_for_data
 *         except transfer ownership of `data`.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] spec The data layout spec to hint.
 * @param [in] spec_length The length of the layout spec buffer.
 *
 * @return The newly allocated object.
 *
 * @memberof CgBuffer
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgBuffer *cg_buffer_new_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    const CgDataSegment *spec,
    guint spec_length) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgBuffer object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgBuffer
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgBuffer *cg_buffer_ref (CgBuffer *self);

/*! @brief Release a strong reference
 *         from a @a CgBuffer object.
 *
 * @param [in] self The object.
 *
 * @memberof CgBuffer
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_buffer_unref (gpointer self);

/*! @brief Create a new @a CgTexture
 *         with initial duplicated data.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] width The width of the image.
 * @param [in] height The height of the image.
 * @param [in] format The format of the image data.
 * @param [in] mipmaps The number of mipmaps to generate.
 * @param [in] msaa The number of samples to use.
 *
 * @return The newly allocated object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_new_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    int width,
    int height,
    int format,
    int mipmaps,
    int msaa) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Like @a cg_texture_new_for_data
 *         except transfer ownership of `data`.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] width The width of the image.
 * @param [in] height The height of the image.
 * @param [in] format The format of the image data.
 * @param [in] mipmaps The number of mipmaps to generate.
 * @param [in] msaa The number of samples to use.
 *
 * @return The newly allocated object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_new_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    int width,
    int height,
    int format,
    int mipmaps,
    int msaa) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a new @a CgTexture
 *         with initial duplicated data
 *         as a cubemap.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] image_size The height/width/depth of the cube.
 * @param [in] format The format of the image data.
 *
 * @return The newly allocated object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_new_cubemap_for_data (
    CgGpu *self,
    gconstpointer data,
    gsize size,
    int image_size,
    int format) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Like @a cg_texture_new_cubemap_for_data
 *         except transfer ownership of `data`.
 *
 * @param [in] self The GPU object.
 * @param [in] data The initial data to be stored.
 * @param [in] size The size of the initial data.
 * @param [in] image_size The height/width/depth of the cube.
 * @param [in] format The format of the image data.
 *
 * @return The newly allocated object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_new_cubemap_for_data_take (
    CgGpu *self,
    gpointer data,
    gsize size,
    int image_size,
    int format) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a new @a CgTexture capable
 *         only of holding a depth component.
 *
 * @param [in] self The GPU object.
 * @param [in] width The width of the image.
 * @param [in] height The height of the image.
 * @param [in] msaa The number of samples to use.
 *
 * @return The newly allocated object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_new_depth (
    CgGpu *self,
    int width,
    int height,
    int msaa) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgTexture object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgTexture *cg_texture_ref (CgTexture *self);

/*! @brief Release a strong reference
 *         from a @a CgTexture object.
 *
 * @param [in] self The object.
 *
 * @memberof CgTexture
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_texture_unref (gpointer self);

/*! @brief Create a new @a CgPlan object.
 *
 * @param [in] self The GPU object.
 *
 * @return The newly allocated object.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgPlan *cg_plan_new (CgGpu *self) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgPlan object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgPlan *cg_plan_ref (CgPlan *self);

/*! @brief Release a strong reference
 *         from a @a CgPlan object.
 *
 * @param [in] self The object.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_unref (gpointer self);

/*! @brief Enable configuration for
 *         the next child group.
 *
 * @param [in] self The plan object.
 *
 * This function must always be paired with
 * a following @a cg_plan_push_group
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_begin_config (CgPlan *self);

/*! @brief Add targets to the group's
 *         child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] first_target The first target to
 *        add as specified by @a CG_STATE_TARGET
 * @param [in] ... Remaining target configurations,
 *         terminated with `NULL`.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_targets (
    CgPlan *self,
    const CgValue *first_target,
    ...) G_GNUC_NULL_TERMINATED;

/*! @brief Like @a cg_plan_config_targets
 *         but read a sized buffer instead.
 *
 * @param [in] self The plan object.
 * @param [in] targets A buffer of @a CgValue as
 *        specified by @a CG_STATE_TARGET
 * @param [in] n_targets The length of the buffer.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_targets_v (
    CgPlan *self,
    const CgValue *const *targets,
    guint n_targets);

/*! @brief Set the shader for the group's
 *         child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] shader A shader object.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_shader (
    CgPlan *self,
    CgShader *shader);

/*! @brief Set shader uniform values for the
 *         group's child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] first_keyval The first keyval.
 * @param [in] ... Remaining keyvals, terminated with `NULL`.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_uniforms (
    CgPlan *self,
    const CgValue *first_keyval,
    ...) G_GNUC_NULL_TERMINATED;

/*! @brief Like @a cg_plan_config_uniforms
 *         but read sized buffers instead.
 *
 * @param [in] self The plan object.
 * @param [in] keyvals A buffer of keyvals.
 * @param [in] n_keyvals The length of the buffer.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_uniforms_v (
    CgPlan *self,
    const CgValue *const *keyvals,
    guint n_keyvals);

/*! @brief Override the viewport for the
 *         group's child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] x The x position of the viewport.
 * @param [in] y The y position of the viewport.
 * @param [in] width The width of the viewport.
 * @param [in] height The height of the viewport.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_dest (
    CgPlan *self,
    int x,
    int y,
    int width,
    int height);

/*! @brief Override the write mask for the
 *         group's child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] mask The write bitmask.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_write_mask (
    CgPlan *self,
    guint32 mask);

/*! @brief Override the depth test func for
 *         the group's child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] func The test func enum.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_depth_test_func (
    CgPlan *self,
    int func);

/*! @brief Set whether to use clockwise winding
 *         to determine winding for the group's
 *         child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] clockwise Whether to use clockwise winding.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_clockwise_faces (
    CgPlan *self,
    gboolean clockwise);

/*! @brief Set whether to backface cull for
 *         the group's child render passes.
 *
 * @param [in] self The plan object.
 * @param [in] cull Whether to cull backfaces.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_config_backface_cull (
    CgPlan *self,
    gboolean cull);

/*! @brief End configuration for and
 *         activate the next child group.
 *
 * @param [in] self The plan object.
 *
 * This function must always be paired with
 * a preceding @a cg_plan_begin_config
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_push_group (CgPlan *self);

/*! @brief Initialize and activate a new child
 *         group with a single function call.
 *
 * @param [in] self The plan object.
 * @param [in] first_prop The first state property specifier.
 * @param [in] first_value The first state property value.
 *             The type of this value must match with the
 *             type prescribed by the requested property.
 * @param [in] ... Remaining prop-value pairs,
 *             terminated with `NULL`.
 *
 * This is a useful convenience function for simple
 * groups, where you know at compile time how many
 * parameters need to be set:
 *
 * ```c
 * cg_plan_push_state (
 *     plan,
 *     CG_STATE_TARGET,  CG_TEXTURE (target),
 *     CG_STATE_DEST,    CG_RECT    (0, 0, 1920, 1080),
 *     CG_STATE_UNIFORM, CG_KEYVAL  ("mvp", CG_MAT4 (mvp)),
 *     NULL);
 * // ...
 * cg_plan_pop (plan);
 * ```
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_push_state (
    CgPlan *self,
    int first_prop,
    const CgValue *first_value,
    ...) G_GNUC_NULL_TERMINATED;

/*! @brief Append buffers to be
 *         included in the output.
 *
 * @param [in] self The plan object.
 * @param [in] instances The number of
 *        times to process the buffers.
 * @param [in] first_buffer The first buffer object.
 * @param [in] ... Remaining buffer objects,
 *        terminated with `NULL`.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_append (
    CgPlan *self,
    guint instances,
    CgBuffer *first_buffer,
    ...) G_GNUC_NULL_TERMINATED;

/*! @brief Like @a cg_plan_append but
 *         read a sized buffer instead.
 *
 * @param [in] self The plan object.
 * @param [in] instances The number of
 *        times to process the buffers.
 * @param [in] buffer A buffer of @a CgBuffer .
 * @param [in] n_buffers The length of the buffer.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_append_v (
    CgPlan *self,
    guint instances,
    CgBuffer **buffers,
    guint n_buffers);

/*! @brief Copy a texture to the output.
 *
 * @param [in] self The plan object.
 * @param [in] src The source texture.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_blit (CgPlan *self,
                   CgTexture *src);

/*! @brief Terminate the current child group
 *         and in turn restore the state of the
 *         plan object to before the group was
 *         configured.
 *
 * @param [in] self The plan object.
 * @param [in] n_groups The number of groups to pop.
 *
 * This function must always be paired with
 * a preceding @a cg_plan_push_group or
 * @a cg_plan_push_state .
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_plan_pop_n_groups (
    CgPlan *self,
    guint n_groups);

/*! @brief A convenience macro to pop one group.
 *
 * @param [in] self The plan object.
 *
 * @memberof CgPlan
 *
 */
#define cg_plan_pop(self) \
  cg_plan_pop_n_groups (self, 1)

/*! @brief Convert a plan object and its associated
 *         resources into backend specific instructions.
 *
 * @param [in] self The plan object.
 * @param [out] error The return location
 *        for a recoverable error.
 *
 * This function will invoke the backend, which in turn
 * will attempt to consume and compile the plan into a
 * @a CgCommands object.
 *
 * It is programmer error to call this function such that the
 * last reference to the plan will not be released. If references
 * are still held elsewhere, the function will log a critical
 * error and return `NULL`.
 *
 * @return A newly allocated @a CgCommands object,
 *         or `NULL` if an error occured.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgCommands *cg_plan_unref_to_commands (
    CgPlan *self,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

/*! @brief Create a strong reference to
 *         a @a CgCommands object.
 *
 * @param [in] self The object.
 *
 * @return The newly referenced object.
 *
 * @memberof CgCommands
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
CgCommands *cg_commands_ref (CgCommands *self);

/*! @brief Release a strong reference
 *         from a @a CgCommands object.
 *
 * @param [in] self The object.
 *
 * @memberof CgCommands
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
void cg_commands_unref (gpointer self);

/*! @brief Run commands right now.
 *
 * @param [in] self The commands object.
 * @param [out] error The return location
 *        for a recoverable error.
 *
 * @return Whether an error occured.
 *
 * @memberof CgPlan
 *
 */
CPC_GPU_AVAILABLE_IN_ALL
gboolean cg_commands_dispatch (
    CgCommands *self,
    GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgGpu, cg_gpu_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgPlan, cg_plan_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgShader, cg_shader_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgBuffer, cg_buffer_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgTexture, cg_texture_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CgCommands, cg_commands_unref);

G_END_DECLS

#undef CPC_GPU_INSIDE
