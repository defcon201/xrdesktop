/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_CONTAINER_H_
#define XRD_CONTAINER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include "xrd-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_CONTAINER xrd_container_get_type()
G_DECLARE_FINAL_TYPE (XrdContainer, xrd_container, XRD, CONTAINER, GObject)

typedef enum {
  XRD_CONTAINER_ATTACHMENT_NONE,
  XRD_CONTAINER_ATTACHMENT_HEAD
} XrdContainerAttachment;

typedef enum {
  XRD_CONTAINER_NO_LAYOUT,
  XRD_CONTAINER_HORIZONTAL,
  XRD_CONTAINER_VERTICAL
} XrdContainerLayout;

struct _XrdContainer;

XrdContainer*
xrd_container_new (void);

void
xrd_container_add_window (XrdContainer *self,
                          XrdWindow *window);

void
xrd_container_set_distance (XrdContainer *self, float distance);

GSList *
xrd_container_get_windows (XrdContainer *self);

float
xrd_container_get_distance (XrdContainer *self);

float
xrd_container_get_speed (XrdContainer *self);

void
xrd_container_set_speed (XrdContainer *self,
                         float speed);

void
xrd_container_set_attachment (XrdContainer *self,
                              XrdContainerAttachment attachment);

void
xrd_container_set_layout (XrdContainer *self,
                          XrdContainerLayout layout);

gboolean
xrd_container_step (XrdContainer *self);

G_END_DECLS

#endif /* xrd_container_H_ */
