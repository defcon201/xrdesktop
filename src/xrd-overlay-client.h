/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_OVERLAY_CLIENT_H_
#define XRD_GLIB_OVERLAY_CLIENT_H_

#include <glib-object.h>
#include <gmodule.h>

#include <openvr-action-set.h>

#include "xrd-overlay-pointer.h"
#include "xrd-overlay-pointer-tip.h"
#include "xrd-overlay-manager.h"
#include "xrd-overlay-button.h"
#include "xrd-overlay-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_CLIENT xrd_overlay_client_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayClient, xrd_overlay_client,
                      XRD, OVERLAY_CLIENT, GObject)

typedef struct XrdClickEvent {
  XrdOverlayWindow *window;
  graphene_point_t *position;
  int               button;
  gboolean          state;
} XrdClickEvent;

typedef struct XrdMoveCursorEvent {
  XrdOverlayWindow *window;
  graphene_point_t *position;
} XrdMoveCursorEvent;

typedef struct XrdClientController
{
  XrdOverlayClient *self;
  int               index;
} XrdClientController;

struct _XrdOverlayClient
{
  GObject parent;

  OpenVRContext *context;

  XrdOverlayPointer *pointer[OPENVR_CONTROLLER_COUNT];
  XrdOverlayPointerTip *intersection[OPENVR_CONTROLLER_COUNT];

  XrdOverlayManager *manager;

  XrdClientController left;
  XrdClientController right;

  XrdOverlayButton *button_reset;
  XrdOverlayButton *button_sphere;

  OpenVROverlayUploader *uploader;

  OpenVRActionSet *wm_actions;
  OpenVRActionSet *synth_actions;

  XrdOverlayWindow *hover_window;
  /* hover_position is relative to hover_window */
  graphene_point_t hover_position;

  uint32_t button_press_state;
  graphene_vec3_t scroll_accumulator;

  GHashTable *overlays_to_windows;

  guint poll_event_source_id;

  guint new_overlay_index;
};

XrdOverlayClient *xrd_overlay_client_new (void);

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               uint32_t          width,
                               uint32_t          height);

void
xrd_overlay_client_remove_window (XrdOverlayClient *self,
                                  XrdOverlayWindow *window);

G_END_DECLS

#endif /* XRD_GLIB_OVERLAY_CLIENT_H_ */
