/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_H_
#define XRD_POINTER_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include <graphene.h>

typedef struct _XrdWindow XrdWindow;

G_BEGIN_DECLS

#define XRD_TYPE_POINTER xrd_pointer_get_type()
G_DECLARE_INTERFACE (XrdPointer, xrd_pointer, XRD, POINTER, GObject)

typedef struct {
  float start_offset;
  float length;
  float default_length;
  gboolean visible;
} XrdPointerData;

struct _XrdPointerInterface
{
  GTypeInterface parent;

  void
  (*move) (XrdPointer        *self,
           graphene_matrix_t *transform);

  void
  (*set_length) (XrdPointer *self,
                 float       length);

  XrdPointerData*
  (*get_data) (XrdPointer *self);

  void
  (*set_transformation) (XrdPointer        *self,
                         graphene_matrix_t *matrix);

  void
  (*get_transformation) (XrdPointer        *self,
                         graphene_matrix_t *matrix);

  void
  (*set_selected_window) (XrdPointer *pointer,
                          XrdWindow  *window);

  void
  (*show) (XrdPointer *self);

  void
  (*hide) (XrdPointer *self);
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

XrdPointerData*
xrd_pointer_get_data (XrdPointer *self);

void
xrd_pointer_init (XrdPointer *self);

void
xrd_pointer_set_transformation (XrdPointer        *self,
                                graphene_matrix_t *matrix);

void
xrd_pointer_get_transformation (XrdPointer        *self,
                                graphene_matrix_t *matrix);

void
xrd_pointer_get_ray (XrdPointer     *self,
                     graphene_ray_t *res);

gboolean
xrd_pointer_get_intersection (XrdPointer      *self,
                              XrdWindow       *window,
                              float           *distance,
                              graphene_vec3_t *res);

void
xrd_pointer_set_selected_window (XrdPointer *self,
                                 XrdWindow  *window);

void
xrd_pointer_show (XrdPointer *self);

void
xrd_pointer_hide (XrdPointer *self);

gboolean
xrd_pointer_is_visible (XrdPointer *self);

G_END_DECLS

#endif /* XRD_POINTER_H_ */
