static GBytes *
sanitize_pixbuf_memory (GdkPixbuf *pixbuf,
                        gsize *out_stride)
{
  gsize size = 0;
  const guchar *data = NULL;
  gsize align = 0;
  gsize stride = 0;
  gsize bpp = 0; /* _bytes_ per pixel, not bits */
  int width = 0;
  int height = 0;
  g_autoptr (GBytes) bytes = NULL;
  gsize copy_stride = 0;
  g_autofree guchar *copy = NULL;

  align = G_ALIGNOF (typeof (data));
  stride = gdk_pixbuf_get_rowstride (pixbuf);
  bpp = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  bytes = gdk_pixbuf_read_pixel_bytes (pixbuf);
  data = g_bytes_get_data (bytes, &size);

  if (GPOINTER_TO_SIZE (data) % align == 0
      && stride % align == 0)
    {
      if (out_stride != NULL) *out_stride = stride;
      return g_steal_pointer (&bytes); /* Everything should be good */
    }

  copy_stride = bpp * width;
  copy = g_malloc (copy_stride * height);

  for (int y = 0; y < height; y++)
    memcpy (copy + y * copy_stride,
            data + y * stride,
            bpp * width);

  if (out_stride != NULL) *out_stride = copy_stride;
  return g_bytes_new_take (g_steal_pointer (&copy), copy_stride * height);
}
