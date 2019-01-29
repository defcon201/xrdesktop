/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"

G_DEFINE_TYPE (XrdOverlayWindow, xrd_overlay_window, G_TYPE_OBJECT)

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_overlay_window_finalize;
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
  self->native = NULL;
  self->overlay = NULL;
  self->texture = NULL;
  self->gl_texture = 0;
  self->width = 0;
  self->height = 0;
  self->recreate = TRUE;
}

XrdOverlayWindow *
xrd_overlay_window_new (void)
{
  return (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW, 0);
}

XrdOverlayWindow *
xrd_overlay_window_new_from_data (void)
{
  return (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW, 0);
}

static void
xrd_overlay_window_finalize (GObject *gobject)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);
  if (self->overlay)
    g_object_unref (self->overlay);
  if (self->texture)
    g_object_unref (self->texture);

  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->finalize (gobject);
}
