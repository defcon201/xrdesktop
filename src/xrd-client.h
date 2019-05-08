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
#include "xrd-desktop-cursor.h"
#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"

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

  gboolean
  (*add_button) (XrdClient          *self,
                 XrdWindow         **button,
                 int                 label_count,
                 gchar             **label,
                 graphene_point3d_t *position,
                 GCallback           press_callback,
                 gpointer            press_callback_data);

  GulkanClient *
  (*get_uploader) (XrdClient *self);
};

XrdClient *xrd_client_new (void);

void
xrd_client_add_window (XrdClient *self,
                       XrdWindow *window,
                       gboolean   is_child,
                       gboolean   follow_head);

void
xrd_client_remove_window (XrdClient *self,
                          XrdWindow *window);

gboolean
xrd_client_add_button (XrdClient          *self,
                       XrdWindow         **button,
                       int                 label_count,
                       gchar             **label,
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

OpenVRActionSet *
xrd_client_get_wm_actions (XrdClient *self);

void
xrd_client_set_pin (XrdClient *self,
                    XrdWindow *win,
                    gboolean pin);

void
xrd_client_show_pinned_only (XrdClient *self,
                             gboolean pinned_only);

void
xrd_client_post_openvr_init (XrdClient *self);

XrdInputSynth *
xrd_client_get_input_synth (XrdClient *self);

gboolean
xrd_client_poll_runtime_events (XrdClient *self);

gboolean
xrd_client_poll_input_events (XrdClient *self);

XrdDesktopCursor *
xrd_client_get_cursor (XrdClient *self);

void
xrd_client_submit_cursor_texture (XrdClient     *self,
                                  GulkanClient  *client,
                                  GulkanTexture *texture,
                                  int            hotspot_x,
                                  int            hotspot_y);

void
xrd_client_add_button_callbacks (XrdClient *self,
                                 XrdWindow *button);

void
xrd_client_add_window_callbacks (XrdClient *self,
                                 XrdWindow *window);

void
xrd_client_set_pointer (XrdClient  *self,
                        XrdPointer *pointer,
                        uint32_t    id);

void
xrd_client_set_pointer_tip (XrdClient     *self,
                            XrdPointerTip *pointer,
                            uint32_t       id);

void
xrd_client_set_desktop_cursor (XrdClient        *self,
                               XrdDesktopCursor *cursor);

cairo_surface_t*
xrd_client_create_button_surface (unsigned char *image, uint32_t width,
                                  uint32_t height, int lines,
                                  gchar *const *text);

G_END_DECLS

#endif /* XRD_GLIB_CLIENT_H_ */
