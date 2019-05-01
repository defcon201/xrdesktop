/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-window.h"
#include <gdk/gdk.h>

G_DEFINE_INTERFACE (XrdWindow, xrd_window, G_TYPE_OBJECT)

enum {
  MOTION_NOTIFY_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SHOW,
  DESTROY,
  SCROLL_EVENT,
  KEYBOARD_PRESS_EVENT,
  KEYBOARD_CLOSE_EVENT,
  GRAB_START_EVENT,
  GRAB_EVENT,
  RELEASE_EVENT,
  HOVER_START_EVENT,
  HOVER_EVENT,
  HOVER_END_EVENT,
  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

static void
xrd_window_default_init (XrdWindowInterface *iface)
{
  window_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new ("motion-notify-event",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[SHOW] =
    g_signal_new ("show",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DESTROY] =
    g_signal_new ("destroy",
                   G_TYPE_FROM_INTERFACE (iface),
                     G_SIGNAL_RUN_CLEANUP |
                      G_SIGNAL_NO_RECURSE |
                      G_SIGNAL_NO_HOOKS,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[SCROLL_EVENT] =
    g_signal_new ("scroll-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_CLOSE_EVENT] =
    g_signal_new ("keyboard-close-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[GRAB_START_EVENT] =
    g_signal_new ("grab-start-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[GRAB_EVENT] =
    g_signal_new ("grab-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[RELEASE_EVENT] =
    g_signal_new ("release-event",
                  G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_END_EVENT] =
    g_signal_new ("hover-end-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_EVENT] =
    g_signal_new ("hover-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_START_EVENT] =
    g_signal_new ("hover-start-event",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  GParamSpec *pspec;
  pspec =
      g_param_spec_string ("window-title",
                           "Window Title",
                           "Title of the Window.",
                           NULL  /* default value */,
                           /* TODO: changeable window description
                            * can not change overlay key? */
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_float ("ppm",
                       "Pixels Per Meter",
                       "Pixels Per Meter Setting of this Window.",
                       0.  /* minimum value */,
                       16384. /* maximum value */,
                       450.  /* default value */,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_float ("scaling-factor",
                       "Scaling Factor",
                       "Scaling Factor of this Window.",
                        /* TODO: use gsettings values */
                       .1  /* minimum value */,
                       10. /* maximum value */,
                       1.  /* default value */,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_pointer ("native",
                          "Native Window Handle",
                          "A pointer to an (opaque) native window struct.",
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_uint ("texture-width",
                       "Texture width",
                       "The width of the set texture.",
                       0,
                       32768,
                       0,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_uint ("texture-height",
                       "Texture height",
                       "The height of the set texture.",
                       0,
                       32768,
                       0,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  iface->windows_created = 0;
}

void
_grab_start_cb (gpointer overlay,
                gpointer event,
                gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_START_EVENT], 0, event);
}

void
_grab_cb (gpointer overlay,
          gpointer event,
          gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[GRAB_EVENT], 0, event);
}
void
_release_cb (gpointer overlay,
             gpointer event,
             gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[RELEASE_EVENT], 0, event);
}
void
_hover_end_cb (gpointer overlay,
               gpointer event,
               gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_END_EVENT], 0, event);
}
void
_hover_cb (gpointer overlay,
           gpointer event,
           gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_EVENT], 0, event);
}
void
_hover_start_cb (gpointer overlay,
                 gpointer event,
                 gpointer window)
{
  (void) overlay;
  g_signal_emit (window, window_signals[HOVER_START_EVENT], 0, event);
}

gboolean
xrd_window_set_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->set_transformation_matrix (self, mat);
}

gboolean
xrd_window_get_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->get_transformation_matrix (self, mat);
}

/**
 * xrd_window_submit_texture:
 * @self: The #XrdWindow
 * @client: A GulkanClient, for example an OpenVROverlayUploader.
 * @texture: A GulkanTexture that is created and owned by the caller.
 * For performance reasons it is a good idea for the caller to reuse this
 * texture.
 */
void
xrd_window_submit_texture (XrdWindow *self,
                           GulkanClient *client,
                           GulkanTexture *texture)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->submit_texture (self, client, texture);
}

float
xrd_window_get_scaling_factor (XrdWindow *self)
{
  GValue val = G_VALUE_INIT;
  g_value_init (&val, G_TYPE_FLOAT);
  g_object_get_property (G_OBJECT (self), "scaling-factor", &val);
  return g_value_get_float (&val);
}

float
xrd_window_get_ppm (XrdWindow *self)
{
  GValue val = G_VALUE_INIT;
  g_value_init (&val, G_TYPE_FLOAT);
  g_object_get_property (G_OBJECT (self), "ppm", &val);
  return g_value_get_float (&val);
}

uint32_t
xrd_window_get_texture_width (XrdWindow *self)
{
  GValue val = G_VALUE_INIT;
  g_value_init (&val, G_TYPE_UINT);
  g_object_get_property (G_OBJECT (self), "texture-width", &val);
  return g_value_get_uint (&val);
}

uint32_t
xrd_window_get_texture_height (XrdWindow *self)
{
  GValue val = G_VALUE_INIT;
  g_value_init (&val, G_TYPE_UINT);
  g_object_get_property (G_OBJECT (self), "texture-height", &val);
  return g_value_get_uint (&val);
}

/**
 * xrd_window_pixel_to_meter:
 * @self: The #XrdWindow
 * @pixel: The amount of pixels to convert to meter.
 *
 * Returns: How many meter in world space the amount of pixels occupy, based
 * on the current ppm and scaling setting of this window.
 */
float
xrd_window_pixel_to_meter (XrdWindow *self, int pixel)
{
  return (float)pixel / self->ppm * self->scaling_factor;
}

/**
 * xrd_window_get_width_meter:
 * @self: The #XrdWindow
 * @meters: The width of the #XrdWindow in meters.
 *
 * Returns: The current world space width of the window in meter.
 */
gboolean
xrd_window_get_width_meter (XrdWindow *self, float *meters)
{
  *meters = xrd_window_pixel_to_meter (self, self->texture_width);
  return TRUE;
}

/**
 * xrd_window_get_height_meter:
 * @self: The #XrdWindow
 * @meters: The height of the #XrdWindow in meter.
 *
 * Returns: The current world space height of the window in meter.
 */
gboolean
xrd_window_get_height_meter (XrdWindow *self, float *meters)
{
  *meters = xrd_window_pixel_to_meter (self, self->texture_height);
  return TRUE;
}

/**
 * xrd_window_poll_event:
 * @self: The #XrdWindow
 *
 * Must be called periodically to receive events from this window.
 */
void
xrd_window_poll_event (XrdWindow *self)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->poll_event (self);
}

/**
 * xrd_window_intersects:
 * @self: The #XrdWindow
 * @pointer_transformation_matrix: pose of a pointer ray (like a controller).
 * @intersection_point: The intersection point if there is an intersection.
 *
 * Returns: True if there is an intersection, else false.
 */
gboolean
xrd_window_intersects (XrdWindow   *self,
                       graphene_matrix_t  *pointer_transformation_matrix,
                       graphene_point3d_t *intersection_point)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->intersects (self,
                            pointer_transformation_matrix,
                            intersection_point);
}

/**
 * xrd_window_intersection_to_pixels:
 * @self: The #XrdWindow
 * @intersection_point: intersection point in meters.
 * @size_pixels: size of the window in pixels.
 * @window_coords: coordinates on the window in pixels.
 */
gboolean
xrd_window_intersection_to_pixels (XrdWindow   *self,
                                   graphene_point3d_t *intersection_point,
                                   XrdPixelSize       *size_pixels,
                                   graphene_point_t   *window_coords)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->intersection_to_pixels (self,
                                        intersection_point,
                                        size_pixels,
                                        window_coords);
}

/**
 * xrd_window_intersection_to_2d_offset_meter:
 * @self: The #XrdWindow
 * @intersection_point: intersection point in meters.
 * @offset_center: offset of the intersection point to the center of the window,
 * on the window plane (xy) and in meter.
 */
gboolean
xrd_window_intersection_to_2d_offset_meter (XrdWindow *self,
                                            graphene_point3d_t *intersection_point,
                                            graphene_point_t   *offset_center)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->intersection_to_2d_offset_meter (self,
                                                 intersection_point,
                                                 offset_center);
}

void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_START_EVENT], 0, event);
}


void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event)
{
  g_signal_emit (self, window_signals[GRAB_EVENT], 0, event);
}

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[RELEASE_EVENT], 0, event);
}

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_END_EVENT], 0, event);
}


void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_EVENT], 0, event);
}

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event)
{
  g_signal_emit (self, window_signals[HOVER_START_EVENT], 0, event);
}

/**
 * xrd_window_add_child:
 * @self: The #XrdWindow
 * @child: An already existing window.
 * @offset_center: The offset of the child window's center to the parent
 * window's center in pixels.
 *
 * x axis points right, y axis points up.
 */
void
xrd_window_add_child (XrdWindow *self,
                      XrdWindow *child,
                      graphene_point_t *offset_center)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->add_child (self, child, offset_center);
}

/**
 * xrd_window_set_color:
 * @self: The #XrdWindow
 * @color: RGB  value in [0,1]x[0,1]x[0,1]
 */
void
xrd_window_set_color (XrdWindow *self,
                      graphene_vec3_t *color)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->set_color (self, color);
}

void
xrd_window_set_flip_y (XrdWindow *self,
                       gboolean flip_y)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->set_flip_y (self, flip_y);
}
