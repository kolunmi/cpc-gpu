#define G_LOG_DOMAIN "CpcGpuExample"
#include <cpc-gpu/cpc-gpu.h>
#include <gtk/gtk.h>

#include "pixbuf.h"

#define VERTEX_SHADER                                                   \
  "#version 330\n"                                                      \
  "in vec3 vertexPosition;\n"                                           \
  "in vec3 vertexNormal;\n"                                             \
  "in vec2 vertexTexCoord;\n"                                           \
  "out vec4 fragColor;\n"                                               \
  "out vec2 fragTexCoord;\n"                                            \
  "uniform mat4 normal;\n"                                              \
  "uniform mat4 mvp;\n"                                                 \
  "void main()\n"                                                       \
  "{\n"                                                                 \
  "    fragTexCoord = vertexTexCoord;\n"                                \
  "    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"                  \
  "    fragColor = vec4(vec3(max(dot(normalize(vec3(1.0, 2.0, -2.0)), " \
  "normalize(vec3(normal*vec4(vertexNormal, 1.0)))), 0.1)), 1.0);\n"    \
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

static const CgDataSegment layout[] = {
  {
      .name = "vertexPosition",
      .type = CG_TYPE_FLOAT,
      .num = 3,
  },
  {
      .name = "vertexNormal",
      .type = CG_TYPE_FLOAT,
      .num = 3,
  },
  {
      .name = "vertexTexCoord",
      .type = CG_TYPE_FLOAT,
      .num = 2,
  },
};

static int icon_width = 0;
static int icon_height = 0;
static gboolean icon_has_alpha = FALSE;
static gsize icon_data_size = 0;
static gpointer icon_data = NULL;

static CgGpu *gpu = NULL;
static CgShader *shader = NULL;
static CgBuffer *cube_vertices = NULL;
static CgTexture *icon = NULL;

static float cube_rotation = 180.0;

static gboolean
render (GtkGLArea *area,
        GdkGLContext *context,
        gpointer user_data)
{
  g_autoptr (GError) local_error = NULL;
  int width = 0;
  int height = 0;
  graphene_matrix_t projection = { 0 };
  graphene_vec3_t eye = { 0 };
  graphene_matrix_t view = { 0 };
  graphene_matrix_t transform = { 0 };
  graphene_matrix_t normal = { 0 };
  graphene_matrix_t tmp = { 0 };
  graphene_matrix_t mvp = { 0 };
  float mvp_arr[16] = { 0 };
  float normal_arr[16] = { 0 };
  g_autoptr (CgPlan) plan = NULL;
  g_autoptr (CgCommands) commands = NULL;

  width = gtk_widget_get_width (GTK_WIDGET (area));
  height = gtk_widget_get_height (GTK_WIDGET (area));

  graphene_vec3_init (&eye, 0.0, 0.0, -1.0);
  graphene_matrix_init_look_at (&view, &eye, graphene_vec3_zero (), graphene_vec3_y_axis ());
  graphene_matrix_init_perspective (&projection, 60.0, (float)width / (float)height, 0.01, 500.0);

  graphene_matrix_init_identity (&transform);
  graphene_matrix_rotate (&transform, cube_rotation, graphene_vec3_y_axis ());
  graphene_matrix_rotate (&transform, -25.0, graphene_vec3_x_axis ());
  graphene_matrix_translate (&transform, &GRAPHENE_POINT3D_INIT (0, 0, 5));

  graphene_matrix_inverse (&transform, &tmp);
  graphene_matrix_transpose (&tmp, &normal);
  graphene_matrix_to_float (&normal, normal_arr);

  graphene_matrix_multiply (&transform, &view, &tmp);
  graphene_matrix_multiply (&tmp, &projection, &mvp);
  graphene_matrix_to_float (&mvp, mvp_arr);

  plan = cg_plan_new (gpu);
  cg_plan_push_state (
      plan,
      CG_STATE_DEST, CG_RECT (0, 0, width, height),
      CG_STATE_DEPTH_FUNC, CG_INT (CG_TEST_LEQUAL),
      CG_STATE_WRITE_MASK, CG_UINT (CG_WRITE_MASK_ALL),
      CG_STATE_SHADER, CG_SHADER (shader),
      CG_STATE_UNIFORM, CG_KEYVAL ("mvp", CG_MAT4 (mvp_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("normal", CG_MAT4 (normal_arr)),
      CG_STATE_UNIFORM, CG_KEYVAL ("texture0", CG_TEXTURE (icon)),
      CG_STATE_UNIFORM, CG_KEYVAL ("colDiffuse", CG_VEC4 (1.0, 1.0, 1.0, 1.0)),
      NULL);
  cg_plan_append_vertices (plan, cube_vertices, NULL);
  cg_plan_pop (plan);

  commands = cg_plan_unref_to_commands (g_steal_pointer (&plan), &local_error);
  if (commands == NULL)
    goto err;

  if (!cg_commands_dispatch (commands, &local_error))
    goto err;

  return TRUE;

err:
  if (local_error != NULL)
    gtk_gl_area_set_error (area, local_error);

  return FALSE;
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

  cube_vertices = cg_buffer_new_for_data (gpu, cube, sizeof (cube));
  cg_buffer_hint_layout (cube_vertices, layout, G_N_ELEMENTS (layout));

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
}

static void
adjustment_value_changed (GtkAdjustment *adjustment,
                          GParamSpec *pspec,
                          GtkGLArea *gl_area)
{
  cube_rotation = gtk_adjustment_get_value (adjustment);
  gtk_widget_queue_draw (GTK_WIDGET (gl_area));
}

static void
on_activate (GtkApplication *app)
{
  GtkWidget *window = NULL;
  GtkWidget *paned = NULL;
  GtkAdjustment *adjustment = NULL;
  GtkWidget *scale = NULL;
  GtkWidget *gl_area = NULL;

  window = gtk_application_window_new (app);
  paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  adjustment = gtk_adjustment_new (cube_rotation, 0, 360, 1, 10, 0);
  scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gl_area = gtk_gl_area_new ();

  gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);
  gtk_paned_set_wide_handle (GTK_PANED (paned), TRUE);
  gtk_paned_set_position (GTK_PANED (paned), 0);
  gtk_paned_set_shrink_start_child (GTK_PANED (paned), FALSE);
  gtk_paned_set_shrink_end_child (GTK_PANED (paned), FALSE);

  g_signal_connect (adjustment, "notify::value",
                    G_CALLBACK (adjustment_value_changed), gl_area);

  gtk_gl_area_set_allowed_apis (GTK_GL_AREA (gl_area), GDK_GL_API_GL);
  g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
  g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);

  gtk_window_set_child (GTK_WINDOW (window), paned);
  gtk_paned_set_start_child (GTK_PANED (paned), scale);
  gtk_paned_set_end_child (GTK_PANED (paned), gl_area);

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
      g_error_free (local_error);
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
