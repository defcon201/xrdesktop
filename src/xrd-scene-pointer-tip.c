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
_set_constant_width (XrdPointerTip *tip)
{
  (void) tip;
  //g_warning ("stub: _set_constant_width\n");
}

static void
_set_active (XrdPointerTip *tip,
             gboolean       active)
{
  (void) tip;
  (void) active;
  //g_warning ("stub: _set_active\n");
}

static void
_animate_pulse (XrdPointerTip *tip)
{
  (void) tip;
  g_warning ("stub: _animate_pulse\n");
}

static void
_set_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  xrd_scene_object_set_transformation (XRD_SCENE_OBJECT (self), matrix);
}

static void
_get_transformation (XrdPointerTip     *tip,
                     graphene_matrix_t *matrix)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  graphene_matrix_t transformation =
    xrd_scene_object_get_transformation (XRD_SCENE_OBJECT (self));
  graphene_matrix_init_from_matrix (matrix, &transformation);
}


static void
_show (XrdPointerTip *tip)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = TRUE;
}

static void
_hide (XrdPointerTip *tip)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  XrdSceneObject *obj = XRD_SCENE_OBJECT (self);
  obj->visible = FALSE;
}

static void
_set_width_meters (XrdPointerTip *tip,
                   float          meters)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  XrdSceneWindow *window = XRD_SCENE_WINDOW (self);
  xrd_scene_window_set_width_meters (window, meters);
}

static void
_submit_texture (XrdPointerTip *tip,
                 GulkanClient  *client,
                 GulkanTexture *texture)
{
  XrdScenePointerTip *self = XRD_SCENE_POINTER_TIP (tip);
  XrdSceneWindow *window = XRD_SCENE_WINDOW (self);
  xrd_window_submit_texture (XRD_WINDOW (window), client, texture);
}

static void
xrd_scene_pointer_tip_interface_init (XrdPointerTipInterface *iface)
{
  iface->set_constant_width = _set_constant_width;
  iface->set_active = _set_active;
  iface->animate_pulse = _animate_pulse;
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->show = _show;
  iface->hide = _hide;
  iface->set_width_meters = _set_width_meters;
  iface->submit_texture = _submit_texture;
}
