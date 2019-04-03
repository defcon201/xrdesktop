/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_WINDOW_H_
#define XRD_WINDOW_H_

#include <glib-object.h>

#include <graphene.h>
#include <gulkan-client.h>
#include <gulkan-texture.h>

typedef struct XrdPixelSize
{
  uint32_t width;
  uint32_t height;
} XrdPixelSize;

typedef struct XrdHoverEvent
{
  graphene_point3d_t point;
  graphene_matrix_t  pose;
  float              distance;
  int                controller_index;
} XrdHoverEvent;

typedef struct XrdGrabEvent
{
  graphene_matrix_t  pose;
  int                controller_index;
} XrdGrabEvent;

typedef struct XrdControllerIndexEvent
{
  int index;
} XrdControllerIndexEvent;


G_BEGIN_DECLS

#define XRD_TYPE_WINDOW xrd_window_get_type()

#define XRD_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XRD_TYPE_WINDOW, XrdWindow))
#define XRD_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XRD_TYPE_WINDOW, XrdWindowClass))
#define XRD_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XRD_TYPE_WINDOW))
#define XRD_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XRD_TYPE_WINDOW))
#define XRD_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XRD_TYPE_WINDOW, XrdWindowClass))

typedef struct _XrdWindow      XrdWindow;
typedef struct _XrdWindowClass XrdWindowClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XrdWindow, g_object_unref)

struct _XrdWindowClass
{
  GObjectClass parent;

  gboolean
  (*set_transformation_matrix) (XrdWindow *self,
                                           graphene_matrix_t *mat);

  gboolean
  (*get_transformation_matrix) (XrdWindow *self,
                                          graphene_matrix_t *mat);

  void
  (*submit_texture) (XrdWindow *self,
                                GulkanClient *client,
                                GulkanTexture *texture);

  float
  (*pixel_to_meter) (XrdWindow *self, int pixel);

  gboolean
  (*get_width_meter) (XrdWindow *self, float *meters);

  gboolean
  (*get_height_meter) (XrdWindow *self, float *meters);

  void
  (*poll_event) (XrdWindow *self);

  gboolean
  (*intersects) (XrdWindow   *self,
                 graphene_matrix_t  *pointer_transformation_matrix,
                 graphene_point3d_t *intersection_point);

  gboolean
  (*intersection_to_window_coords) (XrdWindow          *self,
                                    graphene_point3d_t *intersection_point,
                                    XrdPixelSize       *size_pixels,
                                    graphene_point_t   *window_coords);

  gboolean
  (*intersection_to_offset_center) (XrdWindow *self,
                                    graphene_point3d_t *intersection_point,
                                    graphene_point_t   *offset_center);

  void
  (*emit_grab_start) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_grab) (XrdWindow *self, XrdGrabEvent *event);

  void
  (*emit_release) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_hover_end) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*emit_hover) (XrdWindow *self, XrdHoverEvent *event);

  void
  (*emit_hover_start) (XrdWindow *self, XrdControllerIndexEvent *event);

  void
  (*add_child) (XrdWindow *self, XrdWindow *child,
                graphene_point_t *offset_center);

  void
  (*constructed) (GObject *object);

  guint windows_created;
};

GType xrd_window_get_type (void) G_GNUC_CONST;

struct _XrdWindow
{
  GObject parent;
  gpointer native;

  uint32_t texture_width;
  uint32_t texture_height;
  GString *window_title;

  float ppm;
  float scaling_factor;
  graphene_matrix_t vr_transform;

  /* A window that is pinned on top of this window and follows this window's
   * position and scaling */
  XrdWindow *child_window;
  XrdWindow *parent_window;

  graphene_point_t child_offset_center;
};

/*
XrdWindow *
xrd_window_new (gchar *window_title, int width, int height, gpointer native);
*/

gboolean
xrd_window_set_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat);

gboolean
xrd_window_get_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat);

void
xrd_window_submit_texture (XrdWindow    *self,
                           GulkanClient *client,
                           GulkanTexture *texture);

float
xrd_window_pixel_to_meter (XrdWindow *self, int pixel);

gboolean
xrd_window_get_width_meter (XrdWindow *self, float *meters);

gboolean
xrd_window_get_height_meter (XrdWindow *self, float *meters);

void
xrd_window_poll_event (XrdWindow *self);

gboolean
xrd_window_intersects (XrdWindow   *self,
                       graphene_matrix_t  *pointer_transformation_matrix,
                       graphene_point3d_t *intersection_point);

gboolean
xrd_window_intersection_to_window_coords (XrdWindow   *self,
                                          graphene_point3d_t *intersection_point,
                                          XrdPixelSize       *size_pixels,
                                          graphene_point_t   *window_coords);

gboolean
xrd_window_intersection_to_offset_center (XrdWindow *self,
                                          graphene_point3d_t *intersection_point,
                                          graphene_point_t   *offset_center);


void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event);

void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event);

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event);

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event);

void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event);

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event);

void
xrd_window_add_child (XrdWindow *self,
                      XrdWindow *child,
                      graphene_point_t *offset_center);

G_END_DECLS

#endif /* XRD_WINDOW_H_ */
