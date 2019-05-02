/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer.h"

G_DEFINE_INTERFACE (XrdPointer, xrd_pointer, G_TYPE_OBJECT)

static void
xrd_pointer_default_init (XrdPointerInterface *iface)
{
  (void) iface;
}
