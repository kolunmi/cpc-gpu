#define G_LOG_DOMAIN "CpcGpuExample"
#include <cpc-gpu/cpc-gpu.h>
#include <gtk/gtk.h>

#include "pixbuf.h"

#define VERTEX_SHADER                                                   \
  "#version 330\n"                                                      \
  "in vec3 vertexPosition;\n"                                           \
  "in vec3 vertexNormal;\n"                                             \
  "in vec2 vertexTexCoord;\n"                                           \
  "in vec3 instanceOffset;\n"                                           \
  "out vec4 fragColor;\n"                                               \
  "out vec2 fragTexCoord;\n"                                            \
  "uniform mat4 mvp;\n"                                                 \
  "uniform mat4 normal;\n"                                              \
  "uniform mat4 rotation;\n"                                            \
  "void main()\n"                                                       \
  "{\n"                                                                 \
  "    vec3 rotated = vec3(rotation*vec4(vertexPosition, 1.0));\n"      \
  "    gl_Position = mvp*vec4(rotated+instanceOffset, 1.0);\n"          \
  "    vec3 rotatedNormal = vec3(rotation*vec4(vertexNormal, 1.0));\n"  \
  "    fragColor = vec4(vec3(max(dot(normalize(vec3(1.0, 2.0, -2.0)), " \
  "normalize(vec3(normal*vec4(rotatedNormal, 1.0)))), 0.1)), 1.0);\n"   \
  "    fragTexCoord = vertexTexCoord;\n"                                \
  "}\n"

#define FRAGMENT_SHADER                                      \
  "#version 330\n"                                           \
  "in vec4 fragColor;\n"                                     \
  "in vec2 fragTexCoord;\n"                                  \
  "out vec4 finalColor;\n"                                   \
  "uniform sampler2D texture0;\n"                            \
  "uniform vec4 colDiffuse;\n"                               \
  "void main()\n"                                            \
  "{\n"                                                      \
  "    vec4 texelColor = texture(texture0, fragTexCoord);\n" \
  "    finalColor = texelColor*colDiffuse*fragColor;\n"      \
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

static int icon_width = 0;
static int icon_height = 0;
static gboolean icon_has_alpha = FALSE;
static gsize icon_data_size = 0;
static gpointer icon_data = NULL;

static guint timeout_source = 0;
static GTimer *timer = NULL;

static CgGpu *gpu = NULL;
static CgShader *shader = NULL;
static CgTexture *tmp_target = NULL;
static CgTexture *tmp_depth = NULL;
static CgBuffer *cube_vertices = NULL;
static CgBuffer *offsets = NULL;
static CgTexture *icon = NULL;

static float main_rotation = 180.0;
static int width = 3;
static int height = 3;
static int depth = 3;
static double fps = 60.0;
static float cube_rotation_mult = 90.0;

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
  float mvp_arr[16] = { 0 };
  float normal_arr[16] = { 0 };
  graphene_matrix_t rotation = { 0 };
  float rot_arr[16] = { 0 };
  g_autoptr (CgPlan) plan = NULL;
  g_autoptr (CgCommands) commands = NULL;

  screen_width = gtk_widget_get_width (GTK_WIDGET (area));
  screen_height = gtk_widget_get_height (GTK_WIDGET (area));

  graphene_vec3_init (&eye, 0.0, 0.0, -1.0);
  graphene_matrix_init_look_at (&view, &eye, graphene_vec3_zero (), graphene_vec3_y_axis ());
  graphene_matrix_init_perspective (&projection, 60.0, (float)screen_width / (float)screen_height, 0.01, 500.0);

  graphene_matrix_init_identity (&transform);
  graphene_matrix_rotate (&transform, main_rotation, graphene_vec3_y_axis ());
  graphene_matrix_rotate (&transform, -25.0, graphene_vec3_x_axis ());
  graphene_matrix_translate (&transform, &GRAPHENE_POINT3D_INIT (0, 0, MAX (MAX (MAX (width, height), depth) * 4.0, 10.0)));

  graphene_matrix_multiply (&transform, &view, &tmp);
  graphene_matrix_multiply (&tmp, &projection, &mvp);
  graphene_matrix_to_float (&mvp, mvp_arr);

  graphene_matrix_inverse (&transform, &tmp);
  graphene_matrix_transpose (&tmp, &normal);
  graphene_matrix_to_float (&normal, normal_arr);

  graphene_matrix_init_rotate (&rotation, g_timer_elapsed (timer, NULL) * cube_rotation_mult, graphene_vec3_x_axis ());
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
      CG_STATE_SHADER, CG_SHADER (shader),
      NULL);

  cg_plan_push_state (
      plan,
      CG_STATE_WRITE_MASK, CG_UINT (CG_WRITE_MASK_ALL),
      CG_STATE_DEPTH_FUNC, CG_INT (CG_TEST_LEQUAL),
      CG_STATE_TARGET, CG_TUPLE3 (CG_TEXTURE (tmp_target), CG_INT (CG_BLEND_SRC_ALPHA), CG_INT (CG_BLEND_ONE_MINUS_SRC_ALPHA)),
      CG_STATE_TARGET, CG_TEXTURE (tmp_depth),
      CG_STATE_UNIFORM, CG_KEYVAL ("mvp", CG_MAT4 (mvp_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("normal", CG_MAT4 (normal_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("rotation", CG_MAT4 (rot_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("texture0", CG_TEXTURE (icon)),
      CG_STATE_UNIFORM, CG_KEYVAL ("colDiffuse", CG_VEC4 (1.0, 1.0, 1.0, 1.0)),
      NULL);
  cg_plan_append (plan, width * height * depth, cube_vertices, offsets, NULL);
  cg_plan_pop (plan);

  cg_plan_blit (plan, tmp_target);
  cg_plan_pop (plan);

  commands = cg_plan_unref_to_commands (g_steal_pointer (&plan), &local_error);
  if (commands == NULL)
    goto err;

  if (!cg_commands_dispatch (commands, &local_error))
    goto err;

  cg_gpu_release_this_thread (gpu);
  return TRUE;

err:
  if (local_error != NULL)
    gtk_gl_area_set_error (area, local_error);

  cg_gpu_release_this_thread (gpu);
  return FALSE;
}

static void
timeout (GtkGLArea *gl_area)
{
  gtk_gl_area_queue_render (gl_area);
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
      gpu, g_steal_pointer (&icon_data),
      icon_data_size, icon_width, icon_height,
      icon_has_alpha
          ? CG_FORMAT_RGBA8
          : CG_FORMAT_RGB8,
      1, 0);

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
  g_clear_pointer (&cube_vertices, cg_buffer_unref);
  g_clear_pointer (&offsets, cg_buffer_unref);
  g_clear_pointer (&tmp_target, cg_texture_unref);
  g_clear_pointer (&tmp_depth, cg_texture_unref);
  g_clear_pointer (&shader, cg_shader_unref);
  g_clear_pointer (&gpu, cg_gpu_unref);

  if (timeout_source > 0)
    g_source_remove (timeout_source);
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
  if (timeout_source > 0)
    g_source_remove (timeout_source);
  fps = gtk_adjustment_get_value (adjustment);
  if (fps < 1.0)
    timeout_source = 0;
  else
    timeout_source = g_timeout_add ((1.0 / fps) * G_TIME_SPAN_MILLISECOND, (GSourceFunc)timeout, gl_area);
}

static char *
scale_format (GtkScale *scale,
              double value,
              const char *prefix)
{
  return g_strdup_printf ("%s: %.0f", prefix, value);
}

static void
on_activate (GtkApplication *app)
{
  GtkWidget *window = NULL;
  GtkWidget *box = NULL;
  GtkAdjustment *adjustment = NULL;
  GtkWidget *scale = NULL;
  GtkWidget *gl_area = NULL;

  gl_area = gtk_gl_area_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  window = gtk_application_window_new (app);

  adjustment = gtk_adjustment_new (main_rotation, 0, 360, 1, 10, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (rotation_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_format_value_func (GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Rotation", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (width, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (width_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_format_value_func (GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Width", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (height, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (height_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_format_value_func (GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Height", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (depth, 1, 32, 1, 2, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (depth_value_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_format_value_func (GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Depth", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  adjustment = gtk_adjustment_new (fps, 0, 160, 1, 10, 0);
  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (fps_changed), gl_area);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_format_value_func (GTK_SCALE (scale), (GtkScaleFormatValueFunc)scale_format, "Idle FPS", NULL);
  gtk_box_append (GTK_BOX (box), scale);

  gtk_widget_set_hexpand (gl_area, TRUE);
  gtk_gl_area_set_allowed_apis (GTK_GL_AREA (gl_area), GDK_GL_API_GL);
  gtk_gl_area_set_has_depth_buffer (GTK_GL_AREA (gl_area), FALSE);
  g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (gl_area, "unrealize", G_CALLBACK (unrealize), NULL);
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
  gtk_box_append (GTK_BOX (box), gl_area);

  gtk_window_set_default_size (GTK_WINDOW (window), 1500, 600);
  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc,
      char *argv[])
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GBytes) bytes = NULL;
  GtkApplication *app = NULL;

  pixbuf = gdk_pixbuf_new_from_file ("example/Icon.png", &local_error);
  if (pixbuf == NULL)
    {
      g_critical ("Couldn't load 'example/Icon.png': %s "
                  "(Are you are running from the root dir of the repo?)",
                  local_error->message);
      return 1;
    }

  icon_width = gdk_pixbuf_get_width (pixbuf);
  icon_height = gdk_pixbuf_get_height (pixbuf);
  icon_has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  bytes = sanitize_pixbuf_memory (pixbuf, NULL);
  icon_data = g_bytes_unref_to_data (g_steal_pointer (&bytes), &icon_data_size);

  app = gtk_application_new (
      "com.example.CpcGpuExample",
      G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  return g_application_run (G_APPLICATION (app), argc, argv);
}
