/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"
#include <gdk/gdk.h>

G_DEFINE_TYPE (XrdOverlayWindow, xrd_overlay_window, G_TYPE_OBJECT)

enum {
  MOTION_NOTIFY_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SHOW,
  DESTROY,
  SCROLL_EVENT,
  KEYBOARD_PRESS_EVENT,
  KEYBOARD_CLOSE_EVENT,
  GRAB_START_EVENT,
  GRAB_EVENT,
  RELEASE_EVENT,
  HOVER_START_EVENT,
  HOVER_EVENT,
  HOVER_END_EVENT,
  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_overlay_window_finalize;


  window_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new ("motion-notify-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[SHOW] =
    g_signal_new ("show",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DESTROY] =
    g_signal_new ("destroy",
                   G_TYPE_FROM_CLASS (klass),
                     G_SIGNAL_RUN_CLEANUP |
                      G_SIGNAL_NO_RECURSE |
                      G_SIGNAL_NO_HOOKS,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[SCROLL_EVENT] =
    g_signal_new ("scroll-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_CLOSE_EVENT] =
    g_signal_new ("keyboard-close-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[GRAB_START_EVENT] =
    g_signal_new ("grab-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[GRAB_EVENT] =
    g_signal_new ("grab-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[RELEASE_EVENT] =
    g_signal_new ("release-event",
                  G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_END_EVENT] =
    g_signal_new ("hover-end-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_EVENT] =
    g_signal_new ("hover-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_START_EVENT] =
    g_signal_new ("hover-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}
void
_grab_start_cb (OpenVROverlay *overlay,
                gpointer       event,
                gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_START_EVENT], 0, event);
}

void
_grab_cb (OpenVROverlay *overlay,
          gpointer       event,
          gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_EVENT], 0, event);
}
void
_release_cb (OpenVROverlay *overlay,
             gpointer       event,
             gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[RELEASE_EVENT], 0, event);
}
void
_hover_end_cb (OpenVROverlay *overlay,
               gpointer       event,
               gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_END_EVENT], 0, event);
}
void
_hover_cb (OpenVROverlay *overlay,
           gpointer       event,
           gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_EVENT], 0, event);
}
void
_hover_start_cb (OpenVROverlay *overlay,
                 gpointer       event,
                 gpointer       window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_START_EVENT], 0, event);
}

static void
_disconnect_func (gpointer instance, gpointer func)
{
  g_signal_handlers_disconnect_matched (instance,
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        NULL,
                                        func,
                                        NULL);
}

static void
_disconnect_signals (XrdOverlayWindow *self)
{
  if (!self->overlay)
    return;
  _disconnect_func (self->overlay, _grab_start_cb);
  _disconnect_func (self->overlay, _grab_cb);
  _disconnect_func (self->overlay, _release_cb);
  _disconnect_func (self->overlay, _hover_start_cb);
  _disconnect_func (self->overlay, _hover_cb);
  _disconnect_func (self->overlay, _hover_end_cb);
}

static void
_connect_signals (XrdOverlayWindow *self)
{
  if (!self->overlay)
    return;
  g_signal_connect (self->overlay, "grab-start-event",
                    (GCallback) _grab_start_cb, self);
  g_signal_connect (self->overlay, "grab-event",
                    (GCallback) _grab_cb, self);
  g_signal_connect (self->overlay, "release-event",
                    (GCallback) _release_cb, self);
  g_signal_connect (self->overlay, "hover-start-event",
                    (GCallback) _hover_start_cb, self);
  g_signal_connect (self->overlay, "hover-event",
                    (GCallback) _hover_cb, self);
  g_signal_connect (self->overlay, "hover-end-event",
                    (GCallback) _hover_end_cb, self);
}

void
xrd_overlay_window_init_overlay (XrdOverlayWindow *self,
                                OpenVROverlay *overlay,
                                int width,
                                int height)
{
  if (self->overlay)
    {
      _disconnect_signals (self);
      g_object_unref (overlay);
    }

  self->overlay = overlay;

  if (self->overlay)
    {
      _connect_signals (self);
      g_object_ref (overlay);
    }

  self->width = width;
  self->height = height;
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
  (void) self;
}

XrdOverlayWindow *
xrd_overlay_window_new ()
{
  XrdOverlayWindow *self = (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW, 0);
  return self;
}

XrdOverlayWindow *
xrd_overlay_window_new_from_overlay (OpenVROverlay *overlay,
                                     int width,
                                     int height)
{
  XrdOverlayWindow *self = xrd_overlay_window_new ();
  xrd_overlay_window_init_overlay (self, overlay, width, height);
  return self;
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

  _disconnect_signals (self);

  if (self->overlay)
    g_object_unref (self->overlay);
  if (self->texture)
    g_object_unref (self->texture);

  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->finalize (gobject);
}
