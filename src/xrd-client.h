/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_CLIENT_H_
#define XRD_GLIB_CLIENT_H_

#include <glib-object.h>
#include "xrd-window.h"
#include "xrd-overlay-window.h"
#include "xrd-scene-window.h"
#include "xrd-overlay-client.h"
#include "xrd-scene-client.h"

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define XRD_TYPE_CLIENT xrd_client_get_type()
G_DECLARE_DERIVABLE_TYPE (XrdClient, xrd_client, XRD, CLIENT, GObject)

struct _XrdClientClass
{
  GObjectClass parent;

  XrdWindow *
  (*add_window) (XrdClient  *self,
                 const char *title,
                 gpointer    native,
                 float        ppm,
                 gboolean    is_child,
                 gboolean    follow_head);
  void
  (*remove_window) (XrdClient *self,
                    XrdWindow *window);

  gboolean
  (*add_button) (XrdClient          *self,
                 XrdWindow         **button,
                 gchar              *label,
                 graphene_point3d_t *position,
                 GCallback           press_callback,
                 gpointer            press_callback_data);

  XrdWindow *
  (*get_keyboard_window) (XrdClient *self);

  void
  (*save_reset_transform) (XrdClient *self,
                           XrdWindow *window);

  GulkanClient *
  (*get_uploader) (XrdClient *self);

};

XrdClient *xrd_client_new (void);

XrdWindow *
xrd_client_add_window (XrdClient  *self,
                       const char *title,
                       gpointer    native,
                       float        ppm,
                       gboolean    is_child,
                       gboolean    follow_head);
void
xrd_client_remove_window (XrdClient *self,
                          XrdWindow *window);

gboolean
xrd_client_add_button (XrdClient          *self,
                       XrdWindow         **button,
                       gchar              *label,
                       graphene_point3d_t *position,
                       GCallback           press_callback,
                       gpointer            press_callback_data);

XrdWindow *
xrd_client_get_keyboard_window (XrdClient *self);

void
xrd_client_save_reset_transform (XrdClient *self,
                                 XrdWindow *window);

GulkanClient *
xrd_client_get_uploader (XrdClient *self);

void
xrd_client_emit_keyboard_press (XrdClient *self,
                                GdkEventKey *event);

void
xrd_client_emit_click (XrdClient *self,
                       XrdClickEvent *event);

void
xrd_client_emit_move_cursor (XrdClient *self,
                             XrdMoveCursorEvent *event);

void
xrd_client_emit_system_quit (XrdClient *self,
                             GdkEvent *event);


G_END_DECLS

#endif /* XRD_GLIB_CLIENT_H_ */
