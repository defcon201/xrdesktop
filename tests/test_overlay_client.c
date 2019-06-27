/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include "xrd.h"


static GdkPixbuf *
_load_gdk_pixbuf (const gchar* name)
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
  GdkPixbuf *pixbuf = _load_gdk_pixbuf (resource);
  if (pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  GulkanTexture *texture =
    gulkan_client_texture_new_from_pixbuf (gc, pixbuf,
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           true);

  g_object_unref (pixbuf);

  return texture;
}

int
main ()
{
  XrdOverlayClient *client = xrd_overlay_client_new ();

  GulkanClient *gc = xrd_client_get_uploader (XRD_CLIENT (client));

  GulkanTexture *texture = _make_texture (gc, "/res/cat.jpg");

  guint texture_width = gulkan_texture_get_width (texture);
  guint texture_height = gulkan_texture_get_height (texture);

  float ppm = texture_width / 0.25f;

  XrdOverlayWindow *window = xrd_overlay_window_new_from_pixels ("win.",
                                                                 texture_width,
                                                                 texture_height,
                                                                 ppm);

  xrd_client_add_window (XRD_CLIENT (client), XRD_WINDOW (window), TRUE);

  xrd_window_submit_texture (XRD_WINDOW (window), gc, texture);

  GulkanDevice *device = gulkan_client_get_device (gc);
  gulkan_device_wait_idle (device);

  g_print ("bye bye\n");

  g_object_unref (texture);

  // TODO: Ref window in client
  //g_object_unref (window);

  g_object_unref (client);
  return 0;
}
