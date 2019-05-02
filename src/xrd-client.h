/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_CLIENT_H_
#define XRD_GLIB_CLIENT_H_

#include <glib-object.h>
#include <gdk/gdk.h>

#include <openvr-context.h>

#include "xrd-window.h"
#include "xrd-input-synth.h"
#include "xrd-window-manager.h"

G_BEGIN_DECLS

#define XRD_TYPE_CLIENT xrd_client_get_type()
G_DECLARE_DERIVABLE_TYPE (XrdClient, xrd_client, XRD, CLIENT, GObject)

typedef struct _XrdClientController
{
  XrdClient *self;
  int        index;
} XrdClientController;

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

  gboolean
  (*add_button) (XrdClient          *self,
                 XrdWindow         **button,
                 gchar              *label,
                 graphene_point3d_t *position,
                 GCallback           press_callback,
                 gpointer            press_callback_data);

  XrdWindow *
  (*get_keyboard_window) (XrdClient *self);

  GulkanClient *
  (*get_uploader) (XrdClient *self);

  XrdWindow *
  (*get_synth_hovered) (XrdClient *self);

  void
  (*submit_cursor_texture) (XrdClient *self,
                            GulkanClient *client,
                            GulkanTexture *texture,
                            int hotspot_x,
                            int hotspot_y);
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

XrdWindow *
xrd_client_get_synth_hovered (XrdClient *self);

void
xrd_client_submit_cursor_texture (XrdClient *self,
                                  GulkanClient *client,
                                  GulkanTexture *texture,
                                  int hotspot_x,
                                  int hotspot_y);

OpenVRContext *
xrd_client_get_openvr_context (XrdClient *self);

XrdWindowManager *
xrd_client_get_manager (XrdClient *self);

G_END_DECLS

#endif /* XRD_GLIB_CLIENT_H_ */
