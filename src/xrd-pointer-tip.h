/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_TIP_H_
#define XRD_POINTER_TIP_H_

#include <glib-object.h>
#include <graphene.h>

G_BEGIN_DECLS

#define XRD_TYPE_POINTER_TIP xrd_pointer_tip_get_type()
G_DECLARE_INTERFACE (XrdPointerTip, xrd_pointer_tip, XRD, POINTER_TIP, GObject)

struct _XrdPointerTipInterface
{
  GTypeInterface parent;

  void
  (*set_constant_width) (XrdPointerTip *self);

  void
  (*update) (XrdPointerTip      *self,
             graphene_matrix_t  *pose,
             graphene_point3d_t *intersection_point);

  void
  (*set_active) (XrdPointerTip *self,
                 gboolean       active);

  void
  (*init_vulkan) (XrdPointerTip *self);

  void
  (*animate_pulse) (XrdPointerTip *self);

  void
  (*set_transformation) (XrdPointerTip    *self,
                         graphene_matrix_t *matrix);

  void
  (*show) (XrdPointerTip *self);

  void
  (*hide) (XrdPointerTip *self);
};

void
xrd_pointer_tip_set_constant_width (XrdPointerTip *self);

void
xrd_pointer_tip_update (XrdPointerTip      *self,
                        graphene_matrix_t  *pose,
                        graphene_point3d_t *intersection_point);

void
xrd_pointer_tip_set_active (XrdPointerTip *self,
                            gboolean       active);

void
xrd_pointer_tip_init_vulkan (XrdPointerTip *self);

void
xrd_pointer_tip_animate_pulse (XrdPointerTip *self);

void
xrd_pointer_tip_set_transformation (XrdPointerTip    *self,
                                    graphene_matrix_t *matrix);

void
xrd_pointer_tip_show (XrdPointerTip *self);

void
xrd_pointer_tip_hide (XrdPointerTip *self);

G_END_DECLS

#endif /* XRD_POINTER_TIP_H_ */
