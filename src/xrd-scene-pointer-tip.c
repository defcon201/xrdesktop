/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-pointer-tip.h"

G_DEFINE_TYPE (XrdScenePointerTip, xrd_scene_pointer_tip, G_TYPE_OBJECT)

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
  self->index = 1337;
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
