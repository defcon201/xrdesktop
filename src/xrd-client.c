/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-client.h"

enum {
  KEYBOARD_PRESS_EVENT,
  CLICK_EVENT,
  MOVE_CURSOR_EVENT,
  REQUEST_QUIT_EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XrdClient, xrd_client, G_TYPE_OBJECT)

static void
xrd_client_finalize (GObject *gobject);

static void
xrd_client_class_init (XrdClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_client_finalize;

  signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[CLICK_EVENT] =
    g_signal_new ("click-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[MOVE_CURSOR_EVENT] =
    g_signal_new ("move-cursor-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[REQUEST_QUIT_EVENT] =
    g_signal_new ("request-quit-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0, 0);
}


static void
xrd_client_init (XrdClient *self)
{
  (void) self;
}

XrdWindow *
xrd_client_add_window (XrdClient  *self,
                       const char *title,
                       gpointer    native,
                       float        ppm,
                       gboolean    is_child,
                       gboolean    follow_head)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->add_window == NULL)
      return NULL;
  return klass->add_window (self, title, native, ppm,
                            is_child, follow_head);
}

void
xrd_client_remove_window (XrdClient *self,
                          XrdWindow *window)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->remove_window == NULL)
      return;
  return klass->remove_window (self, window);
}

gboolean
xrd_client_add_button (XrdClient          *self,
                       XrdWindow         **button,
                       gchar              *label,
                       graphene_point3d_t *position,
                       GCallback           press_callback,
                       gpointer            press_callback_data)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->add_button == NULL)
      return FALSE;
  return klass->add_button (self, button, label, position,
                            press_callback, press_callback_data);
}

XrdWindow *
xrd_client_get_keyboard_window (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_keyboard_window == NULL)
      return FALSE;
  return klass->get_keyboard_window (self);
}

void
xrd_client_save_reset_transform (XrdClient *self,
                                 XrdWindow *window)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->save_reset_transform == NULL)
      return;
  return klass->save_reset_transform (self, window);
}

GulkanClient *
xrd_client_get_uploader (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_uploader == NULL)
      return FALSE;
  return klass->get_uploader (self);
}

XrdWindow *
xrd_client_get_synth_hovered (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_synth_hovered == NULL)
      return NULL;
  return klass->get_synth_hovered (self);
}

void
xrd_client_emit_keyboard_press (XrdClient *self,
                                GdkEventKey *event)
{
  g_signal_emit (self, signals[KEYBOARD_PRESS_EVENT], 0, event);
}

void
xrd_client_emit_click (XrdClient *self,
                       XrdClickEvent *event)
{
  g_signal_emit (self, signals[CLICK_EVENT], 0, event);
}

void
xrd_client_emit_move_cursor (XrdClient *self,
                             XrdMoveCursorEvent *event)
{
  g_signal_emit (self, signals[MOVE_CURSOR_EVENT], 0, event);
}

void
xrd_client_emit_system_quit (XrdClient *self,
                             GdkEvent *event)
{
  g_signal_emit (self, signals[REQUEST_QUIT_EVENT], 0, event);
}

XrdClient *
xrd_client_new (void)
{
  return (XrdClient*) g_object_new (XRD_TYPE_CLIENT, 0);
}

static void
xrd_client_finalize (GObject *gobject)
{
  XrdClient *self = XRD_CLIENT (gobject);

  (void) self;
}
