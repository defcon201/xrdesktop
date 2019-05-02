/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_TIP_H_
#define XRD_POINTER_TIP_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define XRD_TYPE_POINTER_TIP xrd_pointer_tip_get_type()
G_DECLARE_INTERFACE (XrdPointerTip, xrd_pointer_tip, XRD, POINTER_TIP, GObject)

struct _XrdPointerTipInterface
{
  GTypeInterface parent;
};

G_END_DECLS

#endif /* XRD_POINTER_TIP_H_ */
