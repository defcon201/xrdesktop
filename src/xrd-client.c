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

typedef struct _XrdClientPrivate
{
  GObject parent;
  guint foo;
} XrdClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XrdClient, xrd_client, G_TYPE_OBJECT)

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
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->foo = 0;
}

/**
 * xrd_client_add_window:
 * @self: The #XrdClient
 * @title: An arbitrary title for the window.
 * @native: A user pointer that should be used for associating a native window
 * struct (or wrapper) with the created #XrdWindow.
 * @ppm: The initial pixel per meter setting for this #XrdWindow.
 * @is_child: If true, the window can not be dragged with controllers and will
 * not be otherwise managed by the window manager. For windows that have this
 * attribute set, xrd_window_add_child() should be called on a desired parent
 * window.
 * @follow_head: An #XrdWindow with this attribute will move to keep its
 * current distance from the user and will move to stay in the user's view.
 *
 * Creates an #XrdWindow, puts it under the management of the #XrdWindowManager
 * and returns it.
 */
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

/**
 * xrd_client_remove_window:
 * @self: The #XrdClient
 * @window: The #XrdWindow to remove.
 *
 * Removes an #XrdWindow from the management of the #XrdClient and the
 * #XrdWindowManager.
 * Note that the #XrdWindow will not be destroyed by this function.
 */
void
xrd_client_remove_window (XrdClient *self,
                          XrdWindow *window)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->remove_window == NULL)
      return;
  return klass->remove_window (self, window);
}

/**
 * xrd_client_add_button:
 * @self: The #XrdClient
 * @button: The button (#XrdWindow) that will be created by this function.
 * @label: Text that will be displayed on the button.
 * @position: World space position of the button.
 * @press_callback: A function that will be called when the button is grabbed.
 * @press_callback_data: User pointer passed to @press_callback.
 */
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

/**
 * xrd_client_get_keyboard_window
 * @self: The #XrdClient
 *
 * Returns: The window that is currently used for keyboard input. Can be NULL.
 */
XrdWindow *
xrd_client_get_keyboard_window (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_keyboard_window == NULL)
      return FALSE;
  return klass->get_keyboard_window (self);
}

/**
 * xrd_client_save_reset_transform:
 * @self: The #XrdClient
 * @window: The #XrdWindow to save the current transform for. The reset
 * functionality of #XrdWindowManager will reset the transform of this window
 * to the transform the window has when this function is called.
 */
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

/**
 * xrd_client_get_synth_hovered:
 * @self: The #XrdClient
 *
 * Returns: If the controller used for synthesizing input is hovering over an
 * #XrdWindow, return this window, else NULL.
 */
XrdWindow *
xrd_client_get_synth_hovered (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_synth_hovered == NULL)
      return NULL;
  return klass->get_synth_hovered (self);
}

/**
 * xrd_client_submit_cursor_texture:
 * @self: The #XrdClient
 * @client: A GulkanClient, for example an OpenVROverlayUploader.
 * @texture: A GulkanTexture that is created and owned by the caller.
 * For performance reasons it is a good idea for the caller to reuse this
 * texture.
 * @hotspot_x: The x component of the hotspot.
 * @hotspot_y: The x component of the hotspot.
 *
 * A hotspot of (x, y) means that the hotspot is at x pixels right, y pixels
 * down from the top left corner of the texture.
 */
void
xrd_client_submit_cursor_texture (XrdClient *self,
                                  GulkanClient *client,
                                  GulkanTexture *texture,
                                  int hotspot_x,
                                  int hotspot_y)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->submit_cursor_texture == NULL)
      return;
  return klass->submit_cursor_texture (self, client, texture,
                                       hotspot_x, hotspot_y);
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
  G_OBJECT_CLASS (xrd_client_parent_class)->finalize (gobject);
}
