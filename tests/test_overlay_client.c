/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include "xrd-overlay-client.h"

int
main ()
{
  XrdOverlayClient *client = xrd_overlay_client_new ();
  g_object_unref (client);
  return 0;
}
