#define G_LOG_DOMAIN "CpcGpuExample"
#include <cpc-gpu/cpc-gpu.h>
#include <gtk/gtk.h>

#include "pixbuf.h"

#define VERTEX_SHADER                                                      \
  "#version 330\n"                                                         \
  "in vec3 vertexPosition;\n"                                              \
  "in vec3 vertexNormal;\n"                                                \
  "in vec2 vertexTexCoord;\n"                                              \
  "in vec3 instanceOffset;\n"                                              \
  "out vec3 fragPosition;\n"                                               \
  "out vec4 fragColor;\n"                                                  \
  "out vec2 fragTexCoord;\n"                                               \
  "uniform bool skybox;\n"                                                 \
  "uniform mat4 projection;\n"                                             \
  "uniform mat4 transform;\n"                                              \
  "uniform mat4 mvp;\n"                                                    \
  "uniform mat4 normal;\n"                                                 \
  "uniform mat4 rotation;\n"                                               \
  "void main()\n"                                                          \
  "{\n"                                                                    \
  "    if (skybox) {\n"                                                    \
  "        mat4 rotModel = mat4(mat3(transform));\n"                       \
  "        gl_Position = projection*rotModel*vec4(vertexPosition, 1.0);\n" \
  "        fragColor = vec4(1.0);\n"                                       \
  "    } else {\n"                                                         \
  "        vec3 rotated = vec3(rotation*vec4(vertexPosition, 1.0));\n"     \
  "        gl_Position = mvp*vec4(rotated+instanceOffset, 1.0);\n"         \
  "        vec3 rotatedNormal = vec3(rotation*vec4(vertexNormal, 1.0));\n" \
  "        fragColor = vec4(vec3(max(dot(normalize(vec3(2.0, 1.0, 2.0)), " \
  "normalize(vec3(normal*vec4(rotatedNormal, 1.0)))), 0.1)), 1.0);\n"      \
  "    }\n"                                                                \
  "    fragTexCoord = vertexTexCoord;\n"                                   \
  "    fragPosition = vertexPosition;\n"                                   \
  "}\n"

#define FRAGMENT_SHADER                                               \
  "#version 330\n"                                                    \
  "in vec3 fragPosition;\n"                                           \
  "in vec4 fragColor;\n"                                              \
  "in vec2 fragTexCoord;\n"                                           \
  "out vec4 finalColor;\n"                                            \
  "uniform bool skybox;\n"                                            \
  "uniform sampler2D texture0;\n"                                     \
  "uniform samplerCube environmentMap;\n"                             \
  "uniform vec4 colDiffuse;\n"                                        \
  "void main()\n"                                                     \
  "{\n"                                                               \
  "    if (skybox) {\n"                                               \
  "        vec3 color = texture(environmentMap, fragPosition).rgb;\n" \
  "        finalColor = vec4(color, 1.0);\n"                          \
  "    } else {\n"                                                    \
  "        vec4 texelColor = texture(texture0, fragTexCoord);\n"      \
  "        finalColor = texelColor*colDiffuse*fragColor;\n"           \
  "    }\n"                                                           \
  "}\n"

static const float cube[] = {
  /* px py pz nx ny nz u v */
  -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
  1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
  1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
  1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
  -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
  -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
  -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
  1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
  -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
  -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
  -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
  -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
  1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
  1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
  1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
  -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
  1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
  -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f
};

static const CgDataSegment cube_layout[] = {
  {
      .name = "vertexPosition",
      .type = CG_TYPE_FLOAT,
      .num = 3,
      .instance_rate = 0,
  },
  {
      .name = "vertexNormal",
      .type = CG_TYPE_FLOAT,
      .num = 3,
      .instance_rate = 0,
  },
  {
      .name = "vertexTexCoord",
      .type = CG_TYPE_FLOAT,
      .num = 2,
      .instance_rate = 0,
  },
};

static const CgDataSegment offset_layout[] = {
  {
      .name = "instanceOffset",
      .type = CG_TYPE_FLOAT,
      .num = 3,
      .instance_rate = 1,
  },
};

typedef struct
{
  int width;
  int height;
  gboolean has_alpha;
  gsize data_size;
  gpointer data;
} Image;

static Image icon_img = { 0 };
static Image skybox_img = { 0 };

static guint timeout_source = 0;
static GTimer *timer = NULL;

static guint debug_refresh_source = 0;
static GPtrArray *api_calls = NULL;

static CgGpu *gpu = NULL;
static CgShader *shader = NULL;
static CgTexture *tmp_target = NULL;
static CgTexture *tmp_depth = NULL;
static CgBuffer *cube_vertices = NULL;
static CgBuffer *offsets = NULL;
static CgTexture *icon = NULL;
static CgTexture *skybox = NULL;

static float main_rotation = 180.0;
static int width = 3;
static int height = 3;
static int depth = 3;
static double fps = 60.0;
static float cube_rotation_mult = 60.0;

static gboolean
render (GtkGLArea *area,
        GdkGLContext *context,
        gpointer user_data)
{
  g_autoptr (GError) local_error = NULL;
  int screen_width = 0;
  int screen_height = 0;
  static int last_screen_width = 0;
  static int last_screen_height = 0;
  graphene_matrix_t projection = { 0 };
  graphene_vec3_t eye = { 0 };
  graphene_matrix_t view = { 0 };
  graphene_matrix_t transform = { 0 };
  graphene_matrix_t normal = { 0 };
  graphene_matrix_t tmp = { 0 };
  graphene_matrix_t mvp = { 0 };
  float projection_arr[16] = { 0 };
  float transform_arr[16] = { 0 };
  float mvp_arr[16] = { 0 };
  float normal_arr[16] = { 0 };
  graphene_matrix_t rotation = { 0 };
  float rot_arr[16] = { 0 };
  g_autoptr (CgPlan) plan = NULL;
  g_autoptr (CgCommands) commands = NULL;

  screen_width = gtk_widget_get_width (GTK_WIDGET (area));
  screen_height = gtk_widget_get_height (GTK_WIDGET (area));

  graphene_vec3_init (&eye, 0.0, 0.0, 1.0);
  graphene_matrix_init_look_at (&view, &eye, graphene_vec3_zero (), graphene_vec3_y_axis ());
  graphene_matrix_init_perspective (&projection, 70.0, (float)screen_width / (float)screen_height, 0.01, 500.0);
  graphene_matrix_to_float (&projection, projection_arr);

  graphene_matrix_init_identity (&transform);
  graphene_matrix_rotate (&transform, main_rotation, graphene_vec3_y_axis ());
  graphene_matrix_rotate (&transform, 5.0, graphene_vec3_x_axis ());
  graphene_matrix_translate (&transform, &GRAPHENE_POINT3D_INIT (0, 0, -MAX (MAX (MAX (width, height), depth) * 6.0, -10.0)));
  graphene_matrix_to_float (&transform, transform_arr);

  graphene_matrix_multiply (&transform, &view, &tmp);
  graphene_matrix_multiply (&tmp, &projection, &mvp);
  graphene_matrix_to_float (&mvp, mvp_arr);

  graphene_matrix_inverse (&transform, &tmp);
  graphene_matrix_transpose (&tmp, &normal);
  graphene_matrix_to_float (&normal, normal_arr);

  graphene_matrix_init_rotate (&rotation, g_timer_elapsed (timer, NULL) * cube_rotation_mult, graphene_vec3_y_axis ());
  graphene_matrix_to_float (&rotation, rot_arr);

  cg_gpu_steal_this_thread (gpu);

  if (tmp_target == NULL
      || last_screen_width != screen_width
      || last_screen_height != screen_height)
    {
      g_clear_pointer (&tmp_target, cg_texture_unref);
      g_clear_pointer (&tmp_depth, cg_texture_unref);

      tmp_target = cg_texture_new_for_data (
          gpu, NULL, 0, screen_width, screen_height,
          CG_FORMAT_RGBA8, 1, 4);
      tmp_depth = cg_texture_new_depth (
          gpu, screen_width, screen_height, 4);

      last_screen_width = screen_width;
      last_screen_height = screen_height;
    }

  if (offsets == NULL)
    {
      gsize size = 0;
      g_autofree float *offsets_buf = NULL;

      size = width * height * depth * 3 * sizeof (float);
      offsets_buf = g_malloc0 (size);

      for (int x = 0; x < width; x++)
        {
          for (int y = 0; y < height; y++)
            {
              for (int z = 0; z < depth; z++)
                {
                  guint idx = 3 * (x * height * depth + y * depth + z);
                  offsets_buf[idx + 0] = 3.0 * ((float)x - (float)(width - 1) / 2.0);
                  offsets_buf[idx + 1] = 3.0 * ((float)y - (float)(height - 1) / 2.0);
                  offsets_buf[idx + 2] = 3.0 * ((float)z - (float)(depth - 1) / 2.0);
                }
            }
        }

      offsets = cg_buffer_new_for_data_take (
          gpu, g_steal_pointer (&offsets_buf), size,
          offset_layout, G_N_ELEMENTS (offset_layout));
    }

  plan = cg_plan_new (gpu);

  cg_plan_push_state (
      plan,
      CG_STATE_DEST, CG_RECT (0, 0, screen_width, screen_height),
      CG_STATE_WRITE_MASK, CG_UINT (CG_WRITE_MASK_COLOR),
      NULL);

  cg_plan_push_state (
      plan,
      CG_STATE_TARGET, CG_TUPLE3 (CG_TEXTURE (tmp_target), CG_INT (CG_BLEND_SRC_ALPHA), CG_INT (CG_BLEND_ONE_MINUS_SRC_ALPHA)),
      CG_STATE_TARGET, CG_TEXTURE (tmp_depth),
      CG_STATE_SHADER, CG_SHADER (shader),
      CG_STATE_UNIFORM, CG_KEYVAL ("projection", CG_MAT4 (projection_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("transform", CG_MAT4 (transform_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("mvp", CG_MAT4 (mvp_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("normal", CG_MAT4 (normal_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("rotation", CG_MAT4 (rot_arr)),
      NULL);

  cg_plan_push_state (
      plan,
      CG_STATE_WRITE_MASK, CG_UINT (CG_WRITE_MASK_COLOR),
      CG_STATE_DEPTH_FUNC, CG_INT (CG_TEST_ALWAYS),
      CG_STATE_BACKFACE_CULL, CG_BOOL (FALSE),
      CG_STATE_UNIFORM, CG_KEYVAL ("skybox", CG_BOOL (TRUE)),
      CG_STATE_UNIFORM, CG_KEYVAL ("environmentMap", CG_TEXTURE (skybox)),
      NULL);
  cg_plan_append (plan, 1, cube_vertices, NULL);
  cg_plan_pop (plan);

  cg_plan_push_state (
      plan,
      CG_STATE_WRITE_MASK, CG_UINT (CG_WRITE_MASK_ALL),
      CG_STATE_DEPTH_FUNC, CG_INT (CG_TEST_LEQUAL),
      CG_STATE_BACKFACE_CULL, CG_BOOL (TRUE),
      CG_STATE_UNIFORM, CG_KEYVAL ("skybox", CG_BOOL (FALSE)),
      CG_STATE_UNIFORM, CG_KEYVAL ("texture0", CG_TEXTURE (icon)),
      CG_STATE_UNIFORM, CG_KEYVAL ("colDiffuse", CG_VEC4 (1.0, 1.0, 1.0, 1.0)),
      NULL);
  cg_plan_append (plan, width * height * depth, cube_vertices, offsets, NULL);
  cg_plan_pop (plan);

  cg_plan_pop (plan);

  cg_plan_blit (plan, tmp_target);
  cg_plan_pop (plan);

  commands = cg_plan_unref_to_debugging_commands (g_steal_pointer (&plan), &local_error);
  if (commands == NULL)
    goto err;

  if (!cg_commands_dispatch (commands, &local_error))
    goto err;

  g_clear_pointer (&api_calls, g_ptr_array_unref);
  api_calls = cg_commands_ref_last_debug_dispatch (commands);

  cg_gpu_release_this_thread (gpu);
  return TRUE;

err:
  if (local_error != NULL)
    gtk_gl_area_set_error (area, local_error);

  cg_gpu_release_this_thread (gpu);
  return FALSE;
}

static gboolean
timeout (GtkGLArea *gl_area)
{
  gtk_gl_area_queue_render (gl_area);
  return G_SOURCE_CONTINUE;
}

static void
realize (GtkGLArea *area,
         gpointer user_data)
{
  g_autoptr (GError) local_error = NULL;

  gtk_gl_area_make_current (area);
  if (gtk_gl_area_get_error (area) != NULL)
    return;

  gpu = cg_gpu_new (
      CG_INIT_FLAG_BACKEND_OPENGL
          | CG_INIT_FLAG_USE_DEBUG_LAYERS
          | CG_INIT_FLAG_EXIT_ON_ERROR
          | CG_INIT_FLAG_LOG_ERRORS,
      NULL, &local_error);
  if (gpu == NULL)
    goto done;

  cg_gpu_steal_this_thread (gpu);

  shader = cg_shader_new_for_code (
      gpu, VERTEX_SHADER, FRAGMENT_SHADER);

  cube_vertices = cg_buffer_new_for_data (
      gpu, cube, sizeof (cube),
      cube_layout, G_N_ELEMENTS (cube_layout));

  icon = cg_texture_new_for_data_take (
      gpu, g_steal_pointer (&icon_img.data),
      icon_img.data_size, icon_img.width, icon_img.height,
      icon_img.has_alpha
          ? CG_FORMAT_RGBA8
          : CG_FORMAT_RGB8,
      1, 0);

  skybox = cg_texture_new_cubemap_for_data_take (
      gpu, g_steal_pointer (&skybox_img.data),
      skybox_img.data_size, skybox_img.width,
      skybox_img.has_alpha
          ? CG_FORMAT_RGBA8
          : CG_FORMAT_RGB8);

done:
  if (local_error != NULL)
    gtk_gl_area_set_error (area, local_error);

  cg_gpu_release_this_thread (gpu);

  timeout_source = g_timeout_add ((1.0 / fps) * G_TIME_SPAN_MILLISECOND, (GSourceFunc)timeout, area);
  timer = g_timer_new ();
}

static void
unrealize (GtkGLArea *area,
           gpointer user_data)
{
  g_clear_pointer (&icon, cg_texture_unref);
  g_clear_pointer (&skybox, cg_texture_unref);
  g_clear_pointer (&cube_vertices, cg_buffer_unref);
  g_clear_pointer (&offsets, cg_buffer_unref);
  g_clear_pointer (&tmp_target, cg_texture_unref);
  g_clear_pointer (&tmp_depth, cg_texture_unref);
  g_clear_pointer (&shader, cg_shader_unref);
  g_clear_pointer (&gpu, cg_gpu_unref);

  g_clear_handle_id (&timeout_source, g_source_remove);
  g_clear_pointer (&timer, g_timer_destroy);
}

static void
rotation_value_changed (GtkAdjustment *adjustment,
                        GParamSpec *pspec,
                        GtkGLArea *gl_area)
{
  main_rotation = gtk_adjustment_get_value (adjustment);
  gtk_gl_area_queue_render (gl_area);
}

static void
width_value_changed (GtkAdjustment *adjustment,
                     GParamSpec *pspec,
                     GtkGLArea *gl_area)
{
  g_clear_pointer (&offsets, cg_buffer_unref);
  width = gtk_adjustment_get_value (adjustment);
  gtk_gl_area_queue_render (gl_area);
}

static void
height_value_changed (GtkAdjustment *adjustment,
                      GParamSpec *pspec,
                      GtkGLArea *gl_area)
{
  g_clear_pointer (&offsets, cg_buffer_unref);
  height = gtk_adjustment_get_value (adjustment);
  gtk_gl_area_queue_render (gl_area);
}

static void
depth_value_changed (GtkAdjustment *adjustment,
                     GParamSpec *pspec,
                     GtkGLArea *gl_area)
{
  g_clear_pointer (&offsets, cg_buffer_unref);
  depth = gtk_adjustment_get_value (adjustment);
  gtk_gl_area_queue_render (gl_area);
}

static void
fps_changed (GtkAdjustment *adjustment,
             GParamSpec *pspec,
             GtkGLArea *gl_area)
{
  g_clear_handle_id (&timeout_source, g_source_remove);
  fps = gtk_adjustment_get_value (adjustment);
  if (fps >= 1.0)
    timeout_source = g_timeout_add (
        (1.0 / fps) * G_TIME_SPAN_MILLISECOND, (GSourceFunc)timeout, gl_area);
}

// static char *
// scale_format (GtkScale *scale,
//               double value,
//               const char *prefix)
// {
//   return g_strdup_printf ("%s: %.0f", prefix, value);
// }

static gboolean
debug_refresh (GtkListBox *list_box)
{
  if (api_calls == NULL)
    return G_SOURCE_CONTINUE;

  gtk_list_box_remove_all (list_box);
  for (guint i = 0; i < api_calls->len; i++)
    {
      GtkWidget *label = NULL;
      label = gtk_label_new (g_ptr_array_index (api_calls, i));
      gtk_widget_add_css_class (label, "monospace");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_list_box_append (list_box, label);
    }

  g_clear_pointer (&api_calls, g_ptr_array_unref);

  return G_SOURCE_CONTINUE;
}

static void
write_to_file_clicked (GtkButton *self,
                       gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileOutputStream) outstream = NULL;

  if (api_calls == NULL)
    return;

  file = g_file_new_for_path ("./api_calls.txt");
  outstream = g_file_replace (
      file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
  if (outstream == NULL)
    goto err;

  for (guint i = 0; i < api_calls->len; i++)
    {
      char *call = NULL;
      gboolean result = FALSE;

      call = g_ptr_array_index (api_calls, i);
      result = g_output_stream_printf (
          G_OUTPUT_STREAM (outstream), NULL, NULL, &error, "%s\n", call);
      if (!result)
        goto err;
    }

  if (!g_output_stream_close (G_OUTPUT_STREAM (outstream), NULL, &error))
    goto err;

  return;

err:
  if (error != NULL)
    g_critical ("Could not write to file: %s", error->message);
}

static void
on_activate (GtkApplication *app)
{
  GtkWidget *window = NULL;
  GtkWidget *box = NULL;
  GtkAdjustment *adjustment = NULL;
  GtkWidget *scale = NULL;
  GtkWidget *vbox = NULL;
  GtkWidget *gl_area = NULL;
  GtkWidget *list_box = NULL;
  GtkWidget *scrolled_window = NULL;
  GtkWidget *button = NULL;

  g_object_set (
      gtk_settings_get_default (),
      "gtk-application-prefer-dark-theme", TRUE,
      NULL);

  gl_area = gtk_gl_area_new ();
  list_box = gtk_list_box_new ();
  scrolled_window = gtk_scrolled_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  window = gtk_application_window_new (app);
  button = gtk_button_new ();

  adjustment = gtk_adjustment_new (main_rotation, 0, 360, 1, 10, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (rotation_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  // gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  // gtk_scale_set_format_value_func (
  //     GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Rotation", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (width, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (width_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  // gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  // gtk_scale_set_format_value_func (
  //     GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Width", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (height, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (height_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  // gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  // gtk_scale_set_format_value_func (
  //     GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Height", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (depth, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (depth_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  // gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  // gtk_scale_set_format_value_func (
  //     GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Depth", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (fps, 0, 160, 1, 10, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (fps_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  // gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  // gtk_scale_set_format_value_func (
  //     GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Idle FPS", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  gtk_widget_set_vexpand (gl_area, TRUE);
  gtk_gl_area_set_allowed_apis (GTK_GL_AREA (gl_area), GDK_GL_API_GL);
  gtk_gl_area_set_has_depth_buffer (GTK_GL_AREA (gl_area), FALSE);
  g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (gl_area, "unrealize", G_CALLBACK (unrealize), NULL);
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
  gtk_box_append (GTK_BOX (vbox), gl_area);

  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

  gtk_widget_set_size_request (scrolled_window, -1, 300);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), list_box);
  debug_refresh_source = g_timeout_add_seconds (1, (GSourceFunc)debug_refresh, list_box);
  gtk_box_append (GTK_BOX (vbox), scrolled_window);

  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

  gtk_button_set_label (GTK_BUTTON (button), "Write to File");
  g_signal_connect (button, "clicked", G_CALLBACK (write_to_file_clicked), NULL);
  gtk_box_append (GTK_BOX (vbox), button);

  gtk_widget_set_hexpand (vbox, TRUE);
  gtk_box_append (GTK_BOX (box), vbox);

  gtk_window_set_default_size (GTK_WINDOW (window), 1600, 1000);
  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_present (GTK_WINDOW (window));
}

static gboolean
init_image (Image *img, const char *path)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GBytes) bytes = NULL;

  pixbuf = gdk_pixbuf_new_from_file (path, &local_error);
  if (pixbuf == NULL)
    {
      g_critical ("Couldn't load '%s': %s "
                  "(Are you are running from the root dir of the repo?)",
                  path, local_error->message);
      return FALSE;
    }

  img->width = gdk_pixbuf_get_width (pixbuf);
  img->height = gdk_pixbuf_get_height (pixbuf);
  img->has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  bytes = sanitize_pixbuf_memory (pixbuf, NULL);
  img->data = g_bytes_unref_to_data (g_steal_pointer (&bytes), &img->data_size);

  return TRUE;
}

int
main (int argc,
      char *argv[])
{
  GtkApplication *app = NULL;

  if (!init_image (&icon_img, "example/Icon.png"))
    return 1;
  if (!init_image (&skybox_img, "example/Skybox.png"))
    return 1;

  app = gtk_application_new (
      "com.example.CpcGpuExample",
      G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  return g_application_run (G_APPLICATION (app), argc, argv);
}
