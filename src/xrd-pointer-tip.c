/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer-tip.h"
#include "xrd-math.h"

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
  iface->set_constant_width (self);
}

void
xrd_pointer_tip_update (XrdPointerTip      *self,
                        graphene_matrix_t  *pose,
                        graphene_point3d_t *intersection_point)
{
  graphene_matrix_t transform;
  graphene_matrix_init_from_matrix (&transform, pose);
  xrd_math_matrix_set_translation_point (&transform, intersection_point);
  xrd_pointer_tip_set_transformation (self, &transform);

  xrd_pointer_tip_set_constant_width (self);
}

void
xrd_pointer_tip_set_active (XrdPointerTip *self,
                            gboolean       active)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->set_active (self, active);
}

void
xrd_pointer_tip_animate_pulse (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->animate_pulse (self);
}

void
xrd_pointer_tip_set_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->set_transformation (self, matrix);
}

void
xrd_pointer_tip_get_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->get_transformation (self, matrix);
}

void
xrd_pointer_tip_show (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->show (self);
}

void
xrd_pointer_tip_hide (XrdPointerTip *self)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->hide (self);
}

void
xrd_pointer_tip_set_width_meters (XrdPointerTip *self,
                                  float          meters)
{
  XrdPointerTipInterface* iface = XRD_POINTER_TIP_GET_IFACE (self);
  iface->set_width_meters (self, meters);
}
