/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>

#include "xrd-scene-client.h"
#include "xrd-overlay-client.h"

#define GRID_WIDTH 4
#define GRID_HEIGHT 4

typedef struct Example
{
  GMainLoop *loop;
  XrdClient *client;
  XrdWindow *head_follow_window;
  XrdWindow *head_follow_button;
  GulkanTexture *hawk_big;
  XrdWindow *switch_button;
  guint64 click_source;
  guint64 move_source;
  guint64 keyboard_source;
  guint64 quit_source;
  guint64 render_source;
  bool shutdown;
} Example;

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

static gboolean
_init_example (Example *example, XrdClient *client);

static void
_cleanup (Example *self);

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
_head_follow_press_cb (XrdWindow               *button,
                       XrdControllerIndexEvent *event,
                       gpointer                 _self)
{
  (void) event;
  (void) button;
  Example *self = _self;
  GulkanClient *gc = xrd_client_get_uploader (self->client);

  if (self->head_follow_window == NULL)
    {
      float ppm = self->hawk_big->width / 0.5;

      if (XRD_IS_SCENE_CLIENT (self->client))
        {
          self->head_follow_window =
            XRD_WINDOW (xrd_scene_window_new_from_ppm ("Head Tracked window.",
                                                       self->hawk_big->width,
                                                       self->hawk_big->height,
                                                       ppm));
          xrd_scene_window_initialize (XRD_SCENE_WINDOW (self->head_follow_window));
        }
      else
        {
          self->head_follow_window =
            XRD_WINDOW (xrd_overlay_window_new_from_ppm ("Head Tracked window.",
                                                         self->hawk_big->width,
                                                         self->hawk_big->height,
                                                         ppm));
          }

      xrd_client_add_window (self->client,
                             self->head_follow_window, FALSE, TRUE);

      xrd_window_submit_texture (self->head_follow_window, gc, self->hawk_big);
      graphene_point3d_t point = { .x = 0, .y = 1, .z = -1.2 };
      graphene_matrix_t transform;
      graphene_matrix_init_translate (&transform, &point);
      xrd_window_set_transformation (self->head_follow_window,
                                            &transform);
      gchar *hide_str[] =  { "Hide", "modal" };
      xrd_button_set_text (self->head_follow_button, gc, 2, hide_str);
    }
  else
    {
      xrd_client_remove_window (XRD_CLIENT (self->client),
                                XRD_WINDOW (self->head_follow_window));
      g_object_unref (self->head_follow_window);
      self->head_follow_window = NULL;
      gchar *show_str[] =  { "Show", "modal" };
      xrd_button_set_text (self->head_follow_button, gc, 2, show_str);
    }
  g_free (event);
}

gboolean
perform_switch (Example *self)
{
  if (XRD_IS_OVERLAY_CLIENT (self->client))
    {
      _cleanup (self);
      _init_example (self, XRD_CLIENT (xrd_scene_client_new ()));
    }
  else
    {
      _cleanup (self);
      _init_example (self, XRD_CLIENT (xrd_overlay_client_new ()));
    }
  return FALSE;
}

void
_button_switch_press_cb (XrdWindow               *window,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) window;
  Example *self = _self;

  g_print ("switch mode\n");
  /* Don't clean up bere because the callback will return.
   * Instead do the cleanup and switch on the next mainloop iteration. */
  g_timeout_add (0, G_SOURCE_FUNC (perform_switch), self);
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
          XrdWindow *window;

          if (XRD_IS_SCENE_CLIENT (self->client))
            {
              window =
                XRD_WINDOW (xrd_scene_window_new_from_ppm ("A window.",
                                                           self->hawk_big->width,
                                                           self->hawk_big->height,
                                                           ppm));
              xrd_scene_window_initialize (XRD_SCENE_WINDOW (window));
            }
          else
            {
              window =
                XRD_WINDOW (xrd_overlay_window_new_from_ppm ("A window.",
                                                             self->hawk_big->width,
                                                             self->hawk_big->height,
                                                             ppm));
            }

          xrd_client_add_window (self->client, window, FALSE, FALSE);

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

          /* TODO: Fix in scene client */
          if (col == 0 && row == 0 && !XRD_IS_SCENE_CLIENT (self->client))
            {
              GulkanTexture *cat_small = _make_texture (gc, "/res/cat.jpg");
              float ppm = cat_small->width / 0.25;
              XrdWindow *child;

              if (XRD_IS_SCENE_CLIENT (self->client))
                {
                  child =
                    XRD_WINDOW (xrd_scene_window_new_from_ppm ("A child.",
                                                               cat_small->width,
                                                               cat_small->height,
                                                               ppm));
                  xrd_scene_window_initialize (XRD_SCENE_WINDOW (child));
                }
              else
                {
                  child =
                    XRD_WINDOW (xrd_overlay_window_new_from_ppm ("A child.",
                                                                 cat_small->width,
                                                                 cat_small->height,
                                                                 ppm));
                }

              xrd_client_add_window (self->client, child, TRUE, FALSE);

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
    .x =  -0.75f,
    .y =  0.0f,
    .z = -1.0f
  };
  gchar *tracked_str[] =  { "Show", "modal"};
  xrd_client_add_button (self->client, &self->head_follow_button,
                         2, tracked_str,
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

  graphene_point3d_t switch_pos = {
    .x =  -0.75f,
    .y =  -xrd_window_get_current_height_meters (self->head_follow_button),
    .z = -1.0f
  };

  gchar *switch_str[] =  { "Switch", "Mode"};
  xrd_client_add_button (self->client, &self->switch_button, 2,
                         switch_str,
                         &switch_pos,
                         (GCallback) _button_switch_press_cb,
                         self);
  return TRUE;
}

static void
_cleanup (Example *self)
{
  self->shutdown = true;
  if (self->render_source != 0)
    g_source_remove (self->render_source);

  g_signal_handler_disconnect (self->client, self->click_source);
  g_signal_handler_disconnect (self->client, self->move_source);
  g_signal_handler_disconnect (self->client, self->keyboard_source);
  g_signal_handler_disconnect (self->client, self->quit_source);
  self->click_source = 0;
  self->move_source = 0;
  self->keyboard_source = 0;
  self->quit_source = 0;
  self->render_source = 0;

  g_object_unref (self->hawk_big);
  self->hawk_big = NULL;
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

gboolean
_iterate_cb (gpointer _self)
{
  Example *self = (Example*) _self;

  if (self->shutdown)
    return FALSE;

  xrd_scene_client_render (XRD_SCENE_CLIENT (self->client));
  return TRUE;
}

static gboolean
_init_example (Example *self, XrdClient *client)
{

  /* TODO: remove special case */
  if (XRD_IS_SCENE_CLIENT (client))
    xrd_scene_client_initialize (XRD_SCENE_CLIENT (client));

  GulkanClient *gc = xrd_client_get_uploader (client);

  self->client = client;
  self->head_follow_button = NULL;
  self->head_follow_window = NULL;
  self->hawk_big = _make_texture (gc, "/res/hawk.jpg");
  self->shutdown = false;

  if (!_init_windows (self))
    return FALSE;

  g_unix_signal_add (SIGINT, _sigint_cb, self);

  self->click_source = g_signal_connect (client, "click-event",
                                         (GCallback) _click_cb, self);
  self->move_source = g_signal_connect (client, "move-cursor-event",
                                        (GCallback) _move_cursor_cb, self);
  self->keyboard_source = g_signal_connect (client, "keyboard-press-event",
                                            (GCallback) _keyboard_press_cb,
                                            self);
  self->quit_source = g_signal_connect (client, "request-quit-event",
                                        (GCallback) _request_quit_cb, self);

  if (XRD_IS_SCENE_CLIENT (client))
    self->render_source = g_timeout_add (1, _iterate_cb, self);

  return TRUE;
}

static gboolean overlay = FALSE;

static GOptionEntry entries[] =
{
  { "overlay", 'o', 0, G_OPTION_ARG_NONE, &overlay, "Launch overlay client by default.", NULL },
  { NULL }
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- xrdesktop client example.");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Wrong parameters: %s\n", error->message);
      exit (1);
    }

  Example self;
  self.loop = g_main_loop_new (NULL, FALSE);

  XrdClient *client;
  if (overlay)
    client = XRD_CLIENT (xrd_overlay_client_new ());
  else
    client = XRD_CLIENT (xrd_scene_client_new ());

  if (!_init_example (&self, client))
    return 1;

  /* start glib main loop */
  g_main_loop_run (self.loop);

  /* don't clean up when quitting during switching */
  if (self.client != NULL)
    _cleanup (&self);

  g_main_loop_unref (self.loop);
}
