/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>

#include <openvr-glib.h>

#include "xrd-overlay-pointer-tip.h"
#include "xrd-settings.h"
#include "xrd-math.h"
#include "graphene-ext.h"
#include "xrd-pointer-tip.h"

struct _XrdOverlayPointerTip
{
  OpenVROverlay parent;

  GulkanClient *gc;

  XrdPointerTipData data;
};

static void
xrd_overlay_pointer_tip_interface_init (XrdPointerTipInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayPointerTip, xrd_overlay_pointer_tip,
                         OPENVR_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_POINTER_TIP,
                                                xrd_overlay_pointer_tip_interface_init))

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject);

static void
xrd_overlay_pointer_tip_class_init (XrdOverlayPointerTipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_pointer_tip_finalize;
}

static void
_set_width_meters (XrdPointerTip *tip, float meters)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_set_width_meters (OPENVR_OVERLAY(self), meters);
}

static void
xrd_overlay_pointer_tip_init (XrdOverlayPointerTip *self)
{
  self->data.active = FALSE;
  self->data.texture = NULL;
  self->data.animation = NULL;
  self->data.upload_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

XrdOverlayPointerTip *
xrd_overlay_pointer_tip_new (guint64 controller_index, GulkanClient *gc)
{
  XrdOverlayPointerTip *self =
    (XrdOverlayPointerTip*) g_object_new (XRD_TYPE_OVERLAY_POINTER_TIP, 0);

  /* our uploader ref needs to stay valid as long as pointer tip exists */
  g_object_ref (gc);
  self->gc = gc;

  char key[k_unVROverlayMaxKeyLength];
  snprintf (key, k_unVROverlayMaxKeyLength - 1, "intersection-%ld",
            controller_index);

  openvr_overlay_create (OPENVR_OVERLAY (self), key, key);

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
    {
      g_printerr ("Intersection overlay unavailable.\n");
      return NULL;
    }

  /*
   * The tip should always be visible, except the pointer can
   * occlude it. The pointer has max sort order, so the tip gets max -1
   */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 1);

  xrd_pointer_tip_init_settings (XRD_POINTER_TIP (self), &self->data);

  return self;
}

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (gobject);
  (void) self;

  /* release the ref we set in pointer tip init */
  g_object_unref (self->gc);
  if (self->data.texture)
    g_object_unref (self->data.texture);

  G_OBJECT_CLASS (xrd_overlay_pointer_tip_parent_class)->finalize (gobject);
}

static void
_set_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), matrix);
}

static void
_get_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self), matrix);
}

static void
_show (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_show (OPENVR_OVERLAY (self));
}

static void
_hide (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  openvr_overlay_hide (OPENVR_OVERLAY (self));
}

static XrdPointerTipData*
_get_data (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  return &self->data;
}

static GulkanClient*
_get_gulkan_client (XrdPointerTip *tip)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  return GULKAN_CLIENT (self->gc);
}

static void
_submit_texture (XrdPointerTip *tip,
                 GulkanClient  *gc,
                 GulkanTexture *texture)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (tip);
  if (!openvr_overlay_submit_texture (OPENVR_OVERLAY (self), gc, texture))
    g_warning ("Could not submit overlay pointer tip texture.\n");
}

static void
xrd_overlay_pointer_tip_interface_init (XrdPointerTipInterface *iface)
{
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
  iface->submit_texture = _submit_texture;
  iface->get_data = _get_data;
  iface->get_gulkan_client = _get_gulkan_client;
}

