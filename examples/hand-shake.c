/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>

#include <xrd.h>

static VkImageLayout upload_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

typedef struct Example
{
  GMainLoop *loop;
  XrdClient *client;
  guint64 click_source;
  guint64 move_source;
  guint64 keyboard_source;
  guint64 quit_source;
  bool shutdown;


  XrdWindow *canvas;
  GdkPixbuf *canvas_pixbuf;
  GulkanTexture *canvas_texture;

  XrdWindow *tutorial_label;
  XrdWindow *result_label;
  XrdWindow *toggle_button;

  int pressed_button;
  graphene_point_t start;
  float max_shake;
} Example;

static gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

static void
_cleanup (Example *self);

static GdkPixbuf *
_create_draw_pixbuf (uint32_t width, uint32_t height)
{
  guchar * pixels = (guchar*) malloc (sizeof (guchar) * height * width * 4);
  memset (pixels, 255, height * width * 4 * sizeof (guchar));

  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
                                                TRUE, 8, (int) width,
                                                (int) height,
                                                4 * (int) width, NULL, NULL);
  return pixbuf;
}

typedef struct ColorRGBA
{
  guchar r;
  guchar g;
  guchar b;
  guchar a;
} ColorRGBA;

static void
_place_pixel (guchar    *pixels,
              int        n_channels,
              int        rowstride,
              int        x,
              int        y,
              ColorRGBA *color)
{
  guchar *p = pixels + y * rowstride
                     + x * n_channels;

  p[0] = color->r;
  p[1] = color->g;
  p[2] = color->b;
  p[3] = color->a;
}

static gboolean
_draw_at_2d_position (Example          *self,
                      graphene_point_t *position_2d,
                      ColorRGBA        *color,
                      uint32_t          brush_radius)
{
  static GMutex paint_mutex;

  int n_channels = gdk_pixbuf_get_n_channels (self->canvas_pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride (self->canvas_pixbuf);
  guchar *pixels = gdk_pixbuf_get_pixels (self->canvas_pixbuf);

  g_mutex_lock (&paint_mutex);

  for (float x = position_2d->x - (float) brush_radius;
       x <= position_2d->x + (float) brush_radius;
       x++)
    {
      for (float y = position_2d->y - (float) brush_radius;
           y <= position_2d->y + (float) brush_radius;
           y++)
        {
          graphene_vec2_t put_position;
          graphene_vec2_init (&put_position, x, y);

          graphene_vec2_t brush_center;
          graphene_point_to_vec2 (position_2d, &brush_center);

          graphene_vec2_t distance;
          graphene_vec2_subtract (&put_position, &brush_center, &distance);

          if (graphene_vec2_length (&distance) < brush_radius)
            _place_pixel (pixels, n_channels, rowstride,
                          (int) x, (int) y, color);

        }
    }

  GulkanClient *gc = xrd_client_get_uploader (self->client);
  if (!gulkan_client_upload_pixbuf (gc, self->canvas_texture,
                                    self->canvas_pixbuf, upload_layout))
    return FALSE;


  xrd_window_submit_texture (self->canvas, gc, self->canvas_texture);

  g_mutex_unlock (&paint_mutex);

  return TRUE;
}

static GdkPixbuf *
load_gdk_pixbuf (const gchar* name)
{
  GError * error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource (name, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, false, 0, 0, 0);
  g_object_unref (pixbuf_rgb);
  return pixbuf;
}

static gboolean
_get_compensation ()
{
  GSettings *s = g_settings_new ("org.xrdesktop");
  gboolean on = g_settings_get_boolean (s, "shake-compensation-enabled");
  return on;
}

static void
_set_compensation (gboolean on)
{
  GSettings *s = g_settings_new ("org.xrdesktop");
  g_settings_set_boolean (s, "shake-compensation-enabled", on);
}


static void
_toggle_press_cb (XrdWindow               *button,
                  XrdControllerIndexEvent *event,
                  gpointer                 _self)
{
  (void) button;
  (void) event;
  Example *self = _self;

  gboolean on = _get_compensation ();
  g_print ("Hand Shake Compensation: %s\n", on ? "ON" : "OFF");

  _set_compensation (!on);

  on = _get_compensation ();
  g_print ("Toggled Hand Shake Compensation to: %s\n", on ? "ON" : "OFF");

  gchar *toggle_string[] = {
    "Comp.",
    on ? "ON" : "OFF"
  };
  GulkanClient *gc = xrd_client_get_uploader (self->client);
  xrd_button_set_text (self->toggle_button, gc,
                       upload_layout, 2, toggle_string);
}

static gboolean
_init_windows (Example *self)
{
  GulkanClient *gc = xrd_client_get_uploader (self->client);

  uint32_t canvas_width = 512;
  uint32_t canvas_height = 512;

  float canvas_width_meter = 1;
  float canvas_height_meter = 1;

  self->canvas_pixbuf =
    _create_draw_pixbuf (canvas_width, canvas_height);

  self->canvas_texture =
    gulkan_client_texture_new_from_pixbuf (gc, self->canvas_pixbuf,
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           upload_layout, true);

  self->canvas =
    XRD_WINDOW (xrd_overlay_window_new_from_meters ("Canvas",
                                                    canvas_width_meter,
                                                    canvas_height_meter));

  xrd_client_add_window (self->client, self->canvas, FALSE);

  xrd_window_submit_texture (self->canvas, gc, self->canvas_texture);

  graphene_point3d_t point = {
    .x = 0,
    .y = canvas_height_meter / 2.f,
    .z = -3
  };
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, &point);
  xrd_window_set_transformation (self->canvas, &transform);
  xrd_window_manager_save_reset_transform (
    xrd_client_get_manager (self->client), self->canvas);


  self->tutorial_label =
    XRD_WINDOW (xrd_overlay_window_new_from_ppm ("Tutorial", 400, 256, 450));

  gchar *tutorial_string[] = {
    "Press A or B below",
    "without shaking"
  };
  xrd_button_set_text (self->tutorial_label, gc,
                       upload_layout, 2, tutorial_string);
  point.y += canvas_height_meter / 2.f + 0.5f / 2.f;
  graphene_matrix_init_translate (&transform, &point);
  xrd_window_set_transformation (self->tutorial_label, &transform);

  gboolean compensation = _get_compensation ();
  gchar *toggle_string[] = {
    "Comp.",
    compensation ? "ON" : "OFF"
  };
  point.x += canvas_width_meter / 2.f + 0.5f / 2.f;

  self->toggle_button =
    xrd_client_button_new_from_text (self->client, 0.5f, 0.5f,
                                     2, toggle_string);

  if (!self->toggle_button)
    return FALSE;

  xrd_client_add_button (self->client, self->toggle_button,
                         &point, (GCallback) _toggle_press_cb, self);

  guint button_pixel_width;
  guint button_pixel_height;
  float button_ppm = xrd_window_get_current_ppm (self->toggle_button);

  g_object_get (self->toggle_button, "texture-width",
                &button_pixel_width, NULL);
  g_object_get (self->toggle_button, "texture-height",
                &button_pixel_height, NULL);
  self->result_label =
    XRD_WINDOW (xrd_overlay_window_new_from_ppm ("Result",
                                                 button_pixel_width,
                                                 button_pixel_height,
                                                 button_ppm));

  gchar *result_string[] = {
    "Shake:",
    "0 Pix"
  };
  xrd_button_set_text (self->result_label, gc, upload_layout, 2, result_string);
  point.y -= xrd_window_get_current_height_meters (self->toggle_button);
  graphene_matrix_init_translate (&transform, &point);
  xrd_window_set_transformation (self->result_label, &transform);



  GdkPixbuf *cursor_pixbuf = load_gdk_pixbuf ("/res/cursor.png");
  if (cursor_pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  GulkanTexture *texture = gulkan_client_texture_new_from_pixbuf (
    gc, cursor_pixbuf, VK_FORMAT_R8G8B8A8_UNORM, upload_layout, true);

  xrd_client_submit_cursor_texture (self->client, gc, texture, 3, 3);

  g_object_unref (cursor_pixbuf);
  g_object_unref (texture);

  return TRUE;
}

static void
_cleanup (Example *self)
{
  self->shutdown = true;

  g_signal_handler_disconnect (self->client, self->click_source);
  g_signal_handler_disconnect (self->client, self->move_source);
  g_signal_handler_disconnect (self->client, self->keyboard_source);
  g_signal_handler_disconnect (self->client, self->quit_source);
  self->click_source = 0;
  self->move_source = 0;
  self->keyboard_source = 0;
  self->quit_source = 0;

  g_object_unref (self->tutorial_label);
  g_object_unref (self->canvas_pixbuf);
  g_object_unref (self->canvas_texture);
  g_object_unref (self->result_label);
  /* wm controls and frees these
  g_object_unref (self->canvas);
  g_object_unref (self->toggle_button);
  */

  g_object_unref (self->client);
  self->client = NULL;
  g_print ("Cleaned up!\n");
}

static void
_click_cb (XrdClient     *client,
           XrdClickEvent *event,
           Example       *self)
{
  (void) client;
  (void) self;
  g_print ("%s: %d at %f, %f\n", event->state ? "click" : "release",
           event->button,
           (double) event->position->x,
           (double) event->position->y);

  self->pressed_button = event->state ? event->button : 0;

  if (self->pressed_button == 0)
    {
      char res[50];
      snprintf (res, 50, "%.1f Pix", (double) self->max_shake);
      gchar *result_string[] = {
        "Shake:",
        res
      };
      GulkanClient *gc = xrd_client_get_uploader (self->client);
      xrd_button_set_text (self->result_label, gc,
                           upload_layout, 2, result_string);
      return;
    }
  else
    {
      graphene_point_init_from_point (&self->start, event->position);
      self->max_shake = 0;
    }

  ColorRGBA color = {
    .r = 0,
    .g = 0,
    .b = 0,
    .a = 255
  };
  _draw_at_2d_position (self, event->position, &color, 5);
}


static void
_move_cursor_cb (XrdClient          *client,
                 XrdMoveCursorEvent *event,
                 Example            *self)
{
  (void) client;
  (void) self;
  (void) event;
  /*
  g_print ("move: %f, %f\n",
           event->position->x, event->position->y);
   */
  if (self->pressed_button == 0)
    return;

  ColorRGBA color = {
    .r = 0,
    .g = 0,
    .b = 0,
    .a = 255
  };
  _draw_at_2d_position (self, event->position, &color, 5);

  float dist = graphene_point_distance (&self->start, event->position, 0, 0);
  self->max_shake = fmaxf (self->max_shake, dist);
}


static void
_keyboard_press_cb (XrdClient   *client,
                    GdkEventKey *event,
                    Example     *self)
{
  (void) client;
  (void) self;
  g_print ("key: %d\n", event->keyval);
}

static void
_request_quit_cb (XrdClient *client,
                  Example   *self)
{
  (void) client;
  (void) self;
  g_print ("Got quit request from the runtime\n");
  g_main_loop_quit (self->loop);
}

int
main ()
{
  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .client = XRD_CLIENT (xrd_overlay_client_new ()),
    .shutdown = false,
    .pressed_button = 0,
  };

  _init_windows (&self);

  self.click_source = g_signal_connect (self.client, "click-event",
                                        (GCallback) _click_cb, &self);
  self.move_source = g_signal_connect (self.client, "move-cursor-event",
                                       (GCallback) _move_cursor_cb, &self);
  self.keyboard_source = g_signal_connect (self.client, "keyboard-press-event",
                                           (GCallback) _keyboard_press_cb,
                                            &self);
  self.quit_source = g_signal_connect (self.client, "request-quit-event",
                                       (GCallback) _request_quit_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);
  g_main_loop_unref (self.loop);
}
