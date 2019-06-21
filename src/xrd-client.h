/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_CLIENT_H_
#define XRD_CLIENT_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include <gdk/gdk.h>

#include <openvr-glib.h>

#include "xrd-window.h"
#include "xrd-input-synth.h"
#include "xrd-window-manager.h"
#include "xrd-desktop-cursor.h"
#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"

G_BEGIN_DECLS

#define XRD_TYPE_CLIENT xrd_client_get_type()
G_DECLARE_DERIVABLE_TYPE (XrdClient, xrd_client, XRD, CLIENT, GObject)

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

  void
  (*init_controller) (XrdClient *self,
                      XrdController *controller);
};

XrdClient *xrd_client_new (void);

void
xrd_client_add_container (XrdClient *self,
                          XrdContainer *container);

void
xrd_client_remove_container (XrdClient *self,
                             XrdContainer *container);

XrdWindow *
xrd_client_window_new_from_meters (XrdClient *client,
                                   const char* title,
                                   float w,
                                   float h);

XrdWindow *
xrd_client_window_new_from_ppm (XrdClient *client,
                                const char* title,
                                uint32_t w,
                                uint32_t h,
                                float ppm);

void
xrd_client_add_window (XrdClient *self,
                       XrdWindow *window,
                       gboolean   draggable);

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
                             OpenVRQuitEvent *event);

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

XrdPointer*
xrd_client_get_pointer (XrdClient  *self,
                        uint32_t    id);

void
xrd_client_set_pointer_tip (XrdClient     *self,
                            XrdPointerTip *pointer,
                            uint32_t       id);

XrdPointerTip*
xrd_client_get_pointer_tip (XrdClient     *self,
                            uint32_t       id);

void
xrd_client_set_desktop_cursor (XrdClient        *self,
                               XrdDesktopCursor *cursor);

XrdDesktopCursor*
xrd_client_get_desktop_cursor (XrdClient *self);

void
xrd_client_set_upload_layout (XrdClient *self, VkImageLayout layout);

VkImageLayout
xrd_client_get_upload_layout (XrdClient *self);

void
xrd_client_init_controller (XrdClient *self,
                            XrdController *controller);

GHashTable *
xrd_client_get_controllers (XrdClient *self);

gboolean
xrd_client_is_hovering (XrdClient *self);

gboolean
xrd_client_is_grabbing (XrdClient *self);

gboolean
xrd_client_is_grabbed (XrdClient *self,
                       XrdWindow *window);

gboolean
xrd_client_is_hovered (XrdClient *self,
                       XrdWindow *window);

GSList *
xrd_client_get_windows (XrdClient *self);

G_END_DECLS

#endif /* XRD_CLIENT_H_ */
