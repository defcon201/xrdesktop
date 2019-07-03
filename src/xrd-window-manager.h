/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_WINDOW_MANAGER_H_
#define XRD_WINDOW_MANAGER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include <openvr-glib.h>

#include "xrd-window.h"
#include "xrd-controller.h"
#include "xrd-container.h"

G_BEGIN_DECLS

#define XRD_TYPE_WINDOW_MANAGER xrd_window_manager_get_type()
G_DECLARE_FINAL_TYPE (XrdWindowManager, xrd_window_manager, XRD,
                      WINDOW_MANAGER, GObject)

typedef struct XrdNoHoverEvent
{
  graphene_matrix_t pose;
  guint64 controller_handle;
} XrdNoHoverEvent;

typedef struct TransformTransition
{
  XrdWindow *window;
  graphene_matrix_t from;
  graphene_matrix_t to;
  float from_scaling;
  float to_scaling;
  float interpolate;
  gint64 last_timestamp;
} TransformTransition;

typedef enum
{
  XRD_WINDOW_HOVERABLE           = 1 << 0,
  XRD_WINDOW_DRAGGABLE           = 1 << 1,
  XRD_WINDOW_MANAGED             = 1 << 2,
  XRD_WINDOW_DESTROY_WITH_PARENT = 1 << 3,
  XRD_WINDOW_MANAGER_BUTTON      = 1 << 4,
} XrdWindowFlags;

typedef enum
{
  XRD_HOVER_MODE_EVERYTHING,
  XRD_HOVER_MODE_BUTTONS
} XrdHoverMode;

XrdWindowManager *xrd_window_manager_new (void);

void
xrd_window_manager_arrange_reset (XrdWindowManager *self);

gboolean
xrd_window_manager_arrange_sphere (XrdWindowManager *self);

void
xrd_window_manager_add_container (XrdWindowManager *self,
                                  XrdContainer *container);

void
xrd_window_manager_remove_container (XrdWindowManager *self,
                                     XrdContainer *container);

void
xrd_window_manager_add_window (XrdWindowManager *self,
                               XrdWindow        *window,
                               XrdWindowFlags    flags);

void
xrd_window_manager_remove_window (XrdWindowManager *self,
                                  XrdWindow        *window);

void
xrd_window_manager_drag_start (XrdWindowManager *self,
                               XrdController *controller);

void
xrd_window_manager_scale (XrdWindowManager *self,
                          GrabState *grab_state,
                          float factor,
                          float update_rate_ms);

void
xrd_window_manager_check_grab (XrdWindowManager *self,
                               XrdController *controller);

void
xrd_window_manager_check_release (XrdWindowManager *self,
                                 XrdController *controller);

void
xrd_window_manager_update_pose (XrdWindowManager *self,
                                graphene_matrix_t *pose,
                                XrdController *controller);

void
xrd_window_manager_poll_window_events (XrdWindowManager *self);

GrabState *
xrd_window_manager_get_grab_state (XrdWindowManager *self,
                                   XrdController *controller);

HoverState *
xrd_window_manager_get_hover_state (XrdWindowManager *self,
                                    XrdController *controller);

GSList *
xrd_window_manager_get_windows (XrdWindowManager *self);

GSList *
xrd_window_manager_get_buttons (XrdWindowManager *self);

void
xrd_window_manager_set_hover_mode (XrdWindowManager *self,
                                   XrdHoverMode mode);

XrdHoverMode
xrd_window_manager_get_hover_mode (XrdWindowManager *self);

G_END_DECLS

#endif /* XRD_WINDOW_MANAGER_H_ */
