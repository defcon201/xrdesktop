/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_H_
#define XRD_POINTER_H_

#include <glib-object.h>
#include <graphene.h>

G_BEGIN_DECLS

#define XRD_TYPE_POINTER xrd_pointer_get_type()
G_DECLARE_INTERFACE (XrdPointer, xrd_pointer, XRD, POINTER, GObject)

struct _XrdPointerInterface
{
  GTypeInterface parent;

  void
  (*move) (XrdPointer        *self,
           graphene_matrix_t *transform);

  void
  (*set_length) (XrdPointer *self,
                 float       length);

  float
  (*get_default_length) (XrdPointer *self);

  void
  (*reset_length) (XrdPointer *self);
};

void
xrd_pointer_move (XrdPointer *self,
                  graphene_matrix_t *transform);

void
xrd_pointer_set_length (XrdPointer *self,
                        float       length);

float
xrd_pointer_get_default_length (XrdPointer *self);

void
xrd_pointer_reset_length (XrdPointer *self);

G_END_DECLS

#endif /* XRD_POINTER_H_ */
