/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-pointer-tip.h"

G_DEFINE_INTERFACE (XrdPointerTip, xrd_pointer_tip, G_TYPE_OBJECT)

static void
xrd_pointer_tip_default_init (XrdPointerTipInterface *iface)
{
  (void) iface;
}
