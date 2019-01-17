/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include "xrd-scene-client.h"

int
main ()
{
  XrdSceneClient *client = xrd_scene_client_new ();
  g_object_unref (client);
  return 0;
}
