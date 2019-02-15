/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>
#include <gmodule.h>

#include "xrd-overlay-client.h"
#include <gdk/gdk.h>

#define GRID_WIDTH 6
#define GRID_HEIGHT 5

typedef struct Example
{
  GMainLoop *loop;
  XrdOverlayClient *client;
  GSList *windows;
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

gboolean
_init_windows (Example *self)
{
  GulkanClient *gc = GULKAN_CLIENT (self->client->uploader);

  GdkPixbuf *pixbuf = load_gdk_pixbuf ("/res/hawk.jpg");
  if (pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  GdkPixbuf *unref = pixbuf;
  pixbuf =
      gdk_pixbuf_scale_simple (pixbuf,
                               (float)gdk_pixbuf_get_width (pixbuf) / 10.,
                               (float)gdk_pixbuf_get_height (pixbuf) / 10.,
                               GDK_INTERP_NEAREST);

  g_object_unref (unref);

  /* TODO: pixels / ppm setting * scaling factor */
  float width = 0.5;

  float pixbuf_aspect = (float) gdk_pixbuf_get_width (pixbuf) /
                        (float) gdk_pixbuf_get_height (pixbuf);

  float height = width / pixbuf_aspect;

  GulkanTexture *texture =
    gulkan_texture_new_from_pixbuf (gc->device, pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_client_upload_pixbuf (gc, texture, pixbuf);

  for (float x = 0; x < GRID_WIDTH * width; x += width)
    for (float y = 0; y < GRID_HEIGHT * height; y += height)
      {
        XrdOverlayWindow *window =
          xrd_overlay_client_add_window (self->client, "A window.", NULL,
                                         gdk_pixbuf_get_width (pixbuf),
                                         gdk_pixbuf_get_height (pixbuf));
        self->windows = g_slist_append (self->windows, window);

        openvr_overlay_uploader_submit_frame (self->client->uploader,
                                              window->overlay, texture);

        graphene_point3d_t point = {
          .x = x,
          .y = y,
          .z = -3
        };
        openvr_overlay_set_translation (window->overlay, &point);

        xrd_overlay_window_manager_save_reset_transform (self->client->manager,
                                                         window);
      }

  return TRUE;
}

void
_cleanup (Example *self)
{
  g_main_loop_unref (self->loop);
  g_slist_free (self->windows);
  g_object_unref (self->client);
  g_print ("bye\n");
}

static void
_click_cb (XrdOverlayClient *client,
           XrdClickEvent    *event,
           Example          *self)
{
  (void) client;
  (void) self;
  g_print ("click: %f, %f\n",
           event->position->x, event->position->y);
}

/*
static void
_move_cursor_cb (XrdOverlayClient   *client,
                 XrdMoveCursorEvent *event,
                 Example            *self)
{
  (void) client;
  (void) self;
  g_print ("move: %f, %f\n",
           event->position->x, event->position->y);
}
*/

static void
_keyboard_press_cb (XrdOverlayClient *client,
                    GdkEventKey      *event,
                    Example          *self)
{
  (void) client;
  (void) self;
  g_print ("key: %d\n", event->keyval);
}

static void
_request_quit_cb (XrdOverlayClient *client,
                  Example          *self)
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
    .client = xrd_overlay_client_new (),
    .windows = NULL
  };

  if (!_init_windows (&self))
    return -1;

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  g_signal_connect (self.client, "click-event",
                    (GCallback) _click_cb, &self);
  //g_signal_connect (self.client, "move-cursor-event",
  //                  (GCallback) _move_cursor_cb, &self);
  g_signal_connect (self.client, "keyboard-press-event",
                    (GCallback) _keyboard_press_cb, &self);

  g_signal_connect (self.client, "request-quit-event",
                    (GCallback) _request_quit_cb, &self);
  
  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);

}
