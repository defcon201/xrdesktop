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

void
xrd_pointer_move (XrdPointer        *self,
                  graphene_matrix_t *transform)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->move (self, transform);
}

void
xrd_pointer_set_length (XrdPointer *self,
                        float       length)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->set_length (self, length);
}

float
xrd_pointer_get_default_length (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->get_default_length (self);
}

void
xrd_pointer_reset_length (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->reset_length (self);
}
