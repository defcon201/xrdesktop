/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_POINTER_TIP_H_
#define XRD_SCENE_POINTER_TIP_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>

#include "xrd-scene-window.h"

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_POINTER_TIP xrd_scene_pointer_tip_get_type()
G_DECLARE_FINAL_TYPE (XrdScenePointerTip, xrd_scene_pointer_tip,
                      XRD, SCENE_POINTER_TIP, XrdSceneWindow)

XrdScenePointerTip *xrd_scene_pointer_tip_new (void);

G_END_DECLS

#endif /* XRD_SCENE_POINTER_TIP_H_ */
