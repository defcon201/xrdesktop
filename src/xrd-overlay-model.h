/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_OVERLAY_MODEL_H_
#define XRD_OVERLAY_MODEL_H_

#include <glib-object.h>

#include "openvr-overlay.h"

G_BEGIN_DECLS

#define XRD_TYPE_OVERLAY_MODEL xrd_overlay_model_get_type()
G_DECLARE_FINAL_TYPE (XrdOverlayModel, xrd_overlay_model, XRD,
                      OVERLAY_MODEL, OpenVROverlay)

struct _XrdOverlayModel
{
  OpenVROverlay parent_type;
};

struct _XrdOverlayModelClass
{
  OpenVROverlayClass parent_class;
};

XrdOverlayModel *xrd_overlay_model_new (gchar* key, gchar* name);

gboolean
xrd_overlay_model_set_model (XrdOverlayModel *self, gchar *name,
                             struct HmdColor_t *color);

gboolean
xrd_overlay_model_get_model (XrdOverlayModel *self, gchar *name,
                             struct HmdColor_t *color, uint32_t *id);

gboolean
xrd_overlay_model_initialize (XrdOverlayModel *self, gchar* key, gchar* name);

G_END_DECLS

#endif /* XRD_OVERLAY_MODEL_H_ */
