/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_H_
#define XRD_POINTER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define XRD_TYPE_POINTER xrd_pointer_get_type()
G_DECLARE_INTERFACE (XrdPointer, xrd_pointer, XRD, POINTER, GObject)

struct _XrdPointerInterface
{
  GTypeInterface parent;
};

G_END_DECLS

#endif /* XRD_POINTER_H_ */
