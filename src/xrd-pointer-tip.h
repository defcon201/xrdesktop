/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_POINTER_TIP_H_
#define XRD_POINTER_TIP_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include <graphene.h>

#include <gulkan.h>

G_BEGIN_DECLS

#define XRD_TYPE_POINTER_TIP xrd_pointer_tip_get_type()
G_DECLARE_INTERFACE (XrdPointerTip, xrd_pointer_tip, XRD, POINTER_TIP, GObject)

/*
 * Since the pulse animation surrounds the tip and would
 * exceed the canvas size, we need to scale it to fit the pulse.
 */
#define XRD_TIP_VIEWPORT_SCALE 3

/*
 * The distance in meters for which apparent size and regular size are equal.
 */
#define XRD_TIP_APPARENT_SIZE_DISTANCE 3.0f

typedef struct XrdPointerTipAnimation
{
  XrdPointerTip *tip;
  float progress;
  guint callback_id;
} XrdPointerTipAnimation;

typedef struct _XrdPointerTipSettings
{
  gboolean keep_apparent_size;
  float width_meters;

  graphene_point3d_t active_color;
  graphene_point3d_t passive_color;

  double pulse_alpha;

  int texture_width;
  int texture_height;
} XrdPointerTipSettings;

typedef struct _XrdPointerTipData
{
  XrdPointerTip *tip;

  gboolean active;

  GulkanTexture *texture;

  XrdPointerTipSettings settings;

  /* Pointer to the data of the currently running animation.
   * Must be freed when an animation callback is cancelled. */
  XrdPointerTipAnimation *animation;
} XrdPointerTipData;

struct _XrdPointerTipInterface
{
  GTypeInterface parent;

  void
  (*set_transformation) (XrdPointerTip     *self,
                         graphene_matrix_t *matrix);

  void
  (*get_transformation) (XrdPointerTip     *self,
                         graphene_matrix_t *matrix);

  void
  (*show) (XrdPointerTip *self);

  void
  (*hide) (XrdPointerTip *self);

  void
  (*set_width_meters) (XrdPointerTip *self,
                       float          meters);

  void
  (*submit_texture) (XrdPointerTip *self,
                     GulkanClient  *client,
                     GulkanTexture *texture);

  XrdPointerTipData*
  (*get_data) (XrdPointerTip *self);

  GulkanClient*
  (*get_gulkan_client) (XrdPointerTip *self);
};

void
xrd_pointer_tip_update_apparent_size (XrdPointerTip *self);

void
xrd_pointer_tip_update (XrdPointerTip      *self,
                        graphene_matrix_t  *pose,
                        graphene_point3d_t *intersection_point);

void
xrd_pointer_tip_set_active (XrdPointerTip *self,
                            gboolean       active);

void
xrd_pointer_tip_animate_pulse (XrdPointerTip *self);

void
xrd_pointer_tip_set_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix);

void
xrd_pointer_tip_get_transformation (XrdPointerTip     *self,
                                    graphene_matrix_t *matrix);

void
xrd_pointer_tip_show (XrdPointerTip *self);

void
xrd_pointer_tip_hide (XrdPointerTip *self);

void
xrd_pointer_tip_set_width_meters (XrdPointerTip *self,
                                  float          meters);

void
xrd_pointer_tip_submit_texture (XrdPointerTip *self,
                                GulkanClient  *client,
                                GulkanTexture *texture);

void
xrd_pointer_tip_init_settings (XrdPointerTip     *self,
                               XrdPointerTipData *data);

GdkPixbuf*
xrd_pointer_tip_render (XrdPointerTip *self,
                        float          progress);

XrdPointerTipData*
xrd_pointer_tip_get_data (XrdPointerTip *self);

GulkanClient*
xrd_pointer_tip_get_gulkan_client (XrdPointerTip *self);

G_END_DECLS

#endif /* XRD_POINTER_TIP_H_ */
