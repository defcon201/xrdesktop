/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <openvr-glib.h>

#include "xrd-window.h"
#include "xrd-overlay-window.h"

int
main ()
{
  OpenVRContext *context = openvr_context_get_instance ();
  if (!openvr_context_init_overlay (context))
    {
      g_printerr ("Error: Could not init OpenVR application.\n");
      return -1;
    }
  if (!openvr_context_is_valid (context))
    {
      g_printerr ("Error: OpenVR context is invalid.\n");
      return -1;
    }

  XrdOverlayWindow *window = xrd_overlay_window_new ("Some window");

  GValue val = G_VALUE_INIT;
  g_value_init (&val, G_TYPE_STRING);

  g_object_get_property (G_OBJECT (window), "title", &val);

  g_print ("Window name: %s\n", g_value_get_string (&val));

  XrdWindow *xrd_window = XRD_WINDOW (window);

  uint32_t texture_width;
  uint32_t texture_height;

  g_object_get (xrd_window,
                "texture-width", &texture_width,
                "texture-height", &texture_height,
                NULL);

  g_print ("XrdWindow texture dimensions %dx%d\n", texture_width, texture_height);

  g_object_unref (window);

  g_object_unref (context);

  return 0;
}
