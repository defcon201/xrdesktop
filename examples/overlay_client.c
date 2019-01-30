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

void
_init_cats (Example *self)
{
  XrdOverlayWindow *window =
    xrd_overlay_client_add_window (self->client, "A cat.", NULL, 10, 10);
  self->windows = g_slist_append (self->windows, window);
}

void
_cleanup (Example *self)
{
  g_main_loop_unref (self->loop);
  g_slist_free (self->windows);
  g_object_unref (self->client);
  g_print ("bye\n");
}

int
main ()
{
  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .client = xrd_overlay_client_new (),
    .windows = NULL
  };

  _init_cats (&self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);

}
