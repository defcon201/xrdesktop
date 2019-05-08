/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>

#include "xrd-overlay-client.h"

#define GRID_WIDTH 5
#define GRID_HEIGHT 5

typedef struct Example
{
  GMainLoop *loop;
  XrdClient *client;
  GSList *windows;
  XrdWindow *head_follow_window;
  XrdWindow *head_follow_button;
  GulkanTexture *hawk_big;
} Example;

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

GdkPixbuf *
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

static GulkanTexture *
_make_texture (GulkanClient *gc, const gchar *resource)
{
  GdkPixbuf *pixbuf = load_gdk_pixbuf (resource);
  if (pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  GulkanTexture *texture =
    gulkan_texture_new_from_pixbuf (gc->device, pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_client_upload_pixbuf (gc, texture, pixbuf);

  g_object_unref (pixbuf);

  return texture;
}

void
_head_follow_press_cb (XrdOverlayWindow        *button,
                       XrdControllerIndexEvent *event,
                       gpointer                 _self)
{
  (void) event;
  (void) button;
  Example *self = _self;
  if (self->head_follow_window == NULL)
    {
      GulkanClient *gc = xrd_client_get_uploader (self->client);

      float ppm = self->hawk_big->width / 0.5;

      self->head_follow_window =
        XRD_WINDOW (xrd_overlay_window_new_from_ppm ("Head Tracked window.",
                                                     self->hawk_big->width,
                                                     self->hawk_big->height,
                                                     ppm));

      xrd_client_add_window (self->client,
                             self->head_follow_window, FALSE, TRUE);

      xrd_window_submit_texture (self->head_follow_window, gc, self->hawk_big);
      graphene_point3d_t point = { .x = 0, .y = 1, .z = -1.2 };
      graphene_matrix_t transform;
      graphene_matrix_init_translate (&transform, &point);
      xrd_window_set_transformation (self->head_follow_window,
                                            &transform);
    }
  else
    {
      xrd_client_remove_window (XRD_CLIENT (self->client),
                                XRD_WINDOW (self->head_follow_window));
      g_object_unref (self->head_follow_window);
      self->head_follow_window = NULL;
    }
  g_free (event);
}

gboolean
_init_windows (Example *self)
{
  GulkanClient *gc = xrd_client_get_uploader (self->client);

  float window_x = 0;
  float window_y = 0;
  for (int col = 0; col < GRID_WIDTH; col++)
    {
      float max_window_height = 0;
      for (int row = 0; row < GRID_HEIGHT; row++)
        {
          // a window should have ~0.5 meter
          float ppm = self->hawk_big->width / 0.5;
          XrdWindow *window =
            XRD_WINDOW (xrd_overlay_window_new_from_ppm ("A window.",
                                                         self->hawk_big->width,
                                                         self->hawk_big->height,
                                                         ppm));

          xrd_client_add_window (self->client, window, FALSE, FALSE);
          self->windows = g_slist_append (self->windows, window);

          xrd_window_submit_texture (window, gc, self->hawk_big);

          window_x += xrd_window_get_current_width_meters (window);

          float window_height = xrd_window_get_current_height_meters (window);
          if (window_height > max_window_height)
            max_window_height = window_height;

          graphene_point3d_t point = {
            .x = window_x,
            .y = window_y,
            .z = -3
          };
          graphene_matrix_t transform;
          graphene_matrix_init_translate (&transform, &point);
          xrd_window_set_transformation (window, &transform);

          XrdWindowManager *manager = xrd_client_get_manager (self->client);
          xrd_window_manager_save_reset_transform (manager, window);

          if (col == 0 && row == 0)
            {
              GulkanTexture *cat_small = _make_texture (gc, "/res/cat.jpg");
              float ppm = cat_small->width / 0.25;
              XrdWindow *child =
                XRD_WINDOW (xrd_overlay_window_new_from_ppm ("A child.",
                                                             cat_small->width,
                                                             cat_small->height,
                                                             ppm));

              xrd_client_add_window (self->client, child, TRUE, FALSE);
              self->windows = g_slist_append (self->windows, child);

              xrd_window_submit_texture (child, gc, cat_small);

              graphene_point_t offset = { .x = 25, .y = 25 };
              xrd_window_add_child (window, child, &offset);

              g_object_unref (cat_small);
            }
        }
      window_x = 0;
      window_y += max_window_height;
    }

  graphene_point3d_t button_position = {
    .x =  0.0f,
    .y =  -0.3f,
    .z = -1.0f
  };
  xrd_client_add_button (self->client, &self->head_follow_button,
                         "Tracked",
                         &button_position,
                         (GCallback) _head_follow_press_cb,
                         self);

  GdkPixbuf *cursor_pixbuf = load_gdk_pixbuf ("/res/cursor.png");
  if (cursor_pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  GulkanTexture *texture = gulkan_texture_new_from_pixbuf (
      gc->device, cursor_pixbuf, VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_pixbuf (gc, texture, cursor_pixbuf);

  xrd_client_submit_cursor_texture (self->client, gc, texture, 3, 3);

  g_object_unref (cursor_pixbuf);
  g_object_unref (texture);

  return TRUE;
}

void
_cleanup (Example *self)
{
  g_main_loop_unref (self->loop);
  g_slist_free (self->windows);
  g_object_unref (self->hawk_big);
  g_object_unref (self->client);
  g_print ("bye\n");
}

static void
_click_cb (XrdClient     *client,
           XrdClickEvent *event,
           Example       *self)
{
  (void) client;
  (void) self;
  g_print ("click: %f, %f\n",
           event->position->x, event->position->y);
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
    .windows = NULL,
    .head_follow_button = NULL,
    .head_follow_window = NULL,
    .hawk_big = NULL,
  };

  GulkanClient *gc = xrd_client_get_uploader (self.client);
  self.hawk_big = _make_texture (gc, "/res/hawk.jpg");


  if (!_init_windows (&self))
    return -1;

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  g_signal_connect (self.client, "click-event",
                    (GCallback) _click_cb, &self);
  g_signal_connect (self.client, "move-cursor-event",
                    (GCallback) _move_cursor_cb, &self);
  g_signal_connect (self.client, "keyboard-press-event",
                    (GCallback) _keyboard_press_cb, &self);

  g_signal_connect (self.client, "request-quit-event",
                    (GCallback) _request_quit_cb, &self);
  
  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);

}
