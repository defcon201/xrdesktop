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
  XrdPointerData *data = xrd_pointer_get_data (self);
  if (length == data->length)
    return;

  data->length = length;

  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->set_length (self, length);
}

float
xrd_pointer_get_default_length (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  return data->default_length;
}

void
xrd_pointer_reset_length (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  xrd_pointer_set_length (self, data->default_length);
}

XrdPointerData*
xrd_pointer_get_data (XrdPointer *self)
{
  XrdPointerInterface* iface = XRD_POINTER_GET_IFACE (self);
  return iface->get_data (self);
}

void
xrd_pointer_init (XrdPointer *self)
{
  XrdPointerData *data = xrd_pointer_get_data (self);
  data->start_offset = -0.02f;
  data->default_length = 5.0f;
  data->length = data->default_length;
}

