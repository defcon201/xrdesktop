/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-scene-renderer.h"

G_DEFINE_TYPE (XrdSceneRenderer, xrd_scene_renderer, GULKAN_TYPE_CLIENT)

static void
xrd_scene_renderer_finalize (GObject *gobject);

static void
xrd_scene_renderer_class_init (XrdSceneRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_scene_renderer_finalize;
}

static void
xrd_scene_renderer_init (XrdSceneRenderer *self)
{
  (void) self;
}

XrdSceneRenderer *
xrd_scene_renderer_new (void)
{
  return (XrdSceneRenderer*) g_object_new (XRD_TYPE_SCENE_RENDERER, 0);
}

static void
xrd_scene_renderer_finalize (GObject *gobject)
{
  XrdSceneRenderer *self = XRD_SCENE_RENDERER (gobject);
  (void) self;

  G_OBJECT_CLASS (xrd_scene_renderer_parent_class)->finalize (gobject);
}

static XrdSceneRenderer *singleton = NULL;

XrdSceneRenderer *xrd_scene_renderer_instance (void)
{
  if (singleton == NULL)
    singleton = xrd_scene_renderer_new ();

  return singleton;
}

