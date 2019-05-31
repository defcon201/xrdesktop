/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>

#include <xrd.h>

static void
_update_input_poll_rate (GSettings *settings, gchar *key, gpointer data)
{
  (void) data;
  guint poll_rate = g_settings_get_uint (settings, key);
  g_assert (poll_rate != 0);
}

int
main ()
{
  xrd_settings_connect_and_apply (G_CALLBACK (_update_input_poll_rate),
                                  "input-poll-rate-ms", NULL);

  return 0;
}
