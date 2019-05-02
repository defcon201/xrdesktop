/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer-tip.h"

G_DEFINE_INTERFACE (XrdPointerTip, xrd_pointer_tip, G_TYPE_OBJECT)

static void
xrd_pointer_tip_default_init (XrdPointerTipInterface *iface)
{
  (void) iface;
}

void
xrd_pointer_tip_set_constant_width (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->set_constant_width (self);
}

void
xrd_pointer_tip_update (XrdPointerTip      *self,
                        graphene_matrix_t  *pose,
                        graphene_point3d_t *intersection_point)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->update (self, pose, intersection_point);
}

void
xrd_pointer_tip_set_active (XrdPointerTip *self,
                            gboolean       active)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->set_active (self, active);
}

void
xrd_pointer_tip_init_vulkan (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->init_vulkan (self);
}

void
xrd_pointer_tip_animate_pulse (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->animate_pulse (self);
}

void
xrd_pointer_tip_set_transformation_matrix (XrdPointerTip    *self,
                                          graphene_matrix_t *matrix)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->set_transformation_matrix (self, matrix);
}

void
xrd_pointer_tip_show (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->show (self);
}

void
xrd_pointer_tip_hide (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  return iface->hide (self);
}
