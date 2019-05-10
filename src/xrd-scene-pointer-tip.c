/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-pointer-tip.h"

#include "xrd-pointer-tip.h"
#include "xrd-math.h"

static void
xrd_scene_pointer_tip_interface_init (XrdPointerTipInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdScenePointerTip, xrd_scene_pointer_tip,
                         XRD_TYPE_SCENE_WINDOW,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_POINTER_TIP,
                                                xrd_scene_pointer_tip_interface_init))

static void
xrd_scene_pointer_tip_finalize (GObject *gobject);

static void
xrd_scene_pointer_tip_class_init (XrdScenePointerTipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_pointer_tip_finalize;
}

static void
xrd_scene_pointer_tip_init (XrdScenePointerTip *self)
{
  (void) self;
}

XrdScenePointerTip *
xrd_scene_pointer_tip_new (void)
{
  return (XrdScenePointerTip*) g_object_new (XRD_TYPE_SCENE_POINTER_TIP, 0);
}

static void
xrd_scene_pointer_tip_finalize (GObject *gobject)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (gobject);
  (void) self;

  G_OBJECT_CLASS (xrd_scene_pointer_tip_parent_class)->finalize (gobject);
}

static void
_set_constant_width (XrdScenePointerTip *self)
{
  (void) self;
  g_warning ("stub: _set_constant_width\n");
}

static void
_update (XrdScenePointerTip *self,
         graphene_matrix_t  *pose,
         graphene_point3d_t *intersection_point)
{
  graphene_matrix_t transform;
  graphene_matrix_init_from_matrix (&transform, pose);
  xrd_math_matrix_set_translation_point (&transform, intersection_point);

  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  xrd_scene_object_set_transformation (obj, &transform);

  // xrd_overlay_pointer_tip_set_constant_width (self);
}

static void
_set_active (XrdScenePointerTip *self,
             gboolean            active)
{
  (void) self;
  (void) active;
  g_warning ("stub: _set_active\n");
}

static void
_init_vulkan (XrdScenePointerTip  *self)
{
  (void) self;
  g_warning ("stub: _init_vulkan\n");
}

static void
_animate_pulse (XrdScenePointerTip  *self)
{
  (void) self;
  g_warning ("stub: _animate_pulse\n");
}

static void
_set_transformation_matrix (XrdScenePointerTip *self,
                            graphene_matrix_t  *matrix)
{
  xrd_scene_object_set_transformation (XRD_SCENE_OBJECT (self), matrix);
}

static void
_show (XrdScenePointerTip *self)
{
  (void) self;
  g_warning ("stub: _show\n");
}

static void
_hide (XrdScenePointerTip *self)
{
  (void) self;
  g_warning ("stub: _hide\n");
}

static void
xrd_scene_pointer_tip_interface_init (XrdPointerTipInterface *iface)
{
  iface->set_constant_width = (void*) _set_constant_width;
  iface->update = (void*) _update;
  iface->set_active = (void*) _set_active;
  iface->init_vulkan = (void*) _init_vulkan;
  iface->animate_pulse = (void*) _animate_pulse;
  iface->set_transformation_matrix = (void*) _set_transformation_matrix;
  iface->show = (void*) _show;
  iface->hide = (void*) _hide;
}
