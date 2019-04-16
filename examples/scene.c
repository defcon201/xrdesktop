/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>

#include "xrd-scene-client.h"

typedef struct Example
{
  XrdSceneClient *client;
  GMainLoop *loop;
  XrdSceneWindow *windows[4];
} Example;

void
_cleanup (Example *self)
{
  g_print ("bye\n");

  g_object_unref (self->client);
}

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

gboolean
_iterate_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  xrd_scene_client_render (self->client);
  return true;
}

GdkPixbuf *
_load_gdk_pixbuf (const gchar* path)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource (path, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }
  else
    {
      GdkPixbuf *pixbuf_rgba = gdk_pixbuf_add_alpha (pixbuf_rgb,
                                                     false, 0, 0, 0);
      g_object_unref (pixbuf_rgb);
      return pixbuf_rgba;
    }
}

gboolean
_init_windows (Example *self)
{
  for (uint32_t i = 0; i < G_N_ELEMENTS (self->windows); i++)
    self->windows[i] = xrd_scene_window_new ();

  GdkPixbuf *pixbufs[2] = {
    _load_gdk_pixbuf ("/res/cat.jpg"),
    _load_gdk_pixbuf ("/res/hawk.jpg"),
  };

  for (uint32_t i = 0; i < G_N_ELEMENTS (pixbufs); i++)
    if (!pixbufs[i])
      return FALSE;

  GulkanClient *client = GULKAN_CLIENT (self->client);

  FencedCommandBuffer cmd_buffer;
  if (!gulkan_client_begin_res_cmd_buffer (client, &cmd_buffer))
    {
      g_printerr ("Could not begin command buffer.\n");
      return false;
    }

  for (uint32_t i = 0; i < G_N_ELEMENTS (self->windows); i++)
    if (!xrd_scene_window_init_texture (self->windows[i], client->device,
                                        cmd_buffer.cmd_buffer,
                                        pixbufs[i % G_N_ELEMENTS (pixbufs)]))
      return FALSE;

  for (uint32_t i = 0; i < G_N_ELEMENTS (pixbufs); i++)
    g_object_unref (pixbufs[i]);

  for (uint32_t i = 0; i < G_N_ELEMENTS (self->windows); i++)
    {
      xrd_scene_window_initialize (self->windows[i],
                                   client->device,
                                   &self->client->descriptor_set_layout);

      graphene_point3d_t position = {
        -1, //i / 2.0f - 1,
        1, //(float) i / 3.0f + 1,
        -(float) i / 3.0f
      };

      XrdSceneObject *obj = XRD_SCENE_OBJECT (self->windows[i]);
      xrd_scene_object_set_position (obj, &position);
      xrd_scene_object_set_scale (obj, (i + 1) * 0.2f);

      graphene_euler_t rotation;
      graphene_euler_init (&rotation, i * 15.0f, 20.0f, 5.0f);
      xrd_scene_object_set_rotation_euler (obj, &rotation);

      xrd_scene_client_add_window (self->client, self->windows[i]);
    }

  if (!gulkan_client_submit_res_cmd_buffer (client, &cmd_buffer))
    {
      g_printerr ("Could not submit command buffer.\n");
      return false;
    }

  return TRUE;
}

int
main ()
{
  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .client = xrd_scene_client_new ()
  };

  if (!xrd_scene_client_initialize (self.client))
    {
      _cleanup (&self);
      return 1;
    }

  if (!_init_windows (&self))
    {
      _cleanup (&self);
      return 1;
    }

  g_timeout_add (1, _iterate_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);
  g_main_loop_unref (self.loop);

  _cleanup (&self);

  return 0;
}
