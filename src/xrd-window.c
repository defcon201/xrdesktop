/*
 * xrdesktop
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
      g_param_spec_string ("title",
                           "Title",
                           "Title of the Window.",
                           "Untitled",
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_float ("scale",
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

  pspec =
    g_param_spec_float ("initial-width-meters",
                       "Initial width (meters)",
                       "Initial window width in meters.",
                       0.01f,
                       1000.0f,
                       1.0f,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  pspec =
    g_param_spec_float ("initial-height-meters",
                       "Initial height (meters)",
                       "Initial window height in meters.",
                       0.01f,
                       1000.0f,
                       1.0f,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_interface_install_property (iface, pspec);

  iface->windows_created = 0;
}

gboolean
xrd_window_set_transformation (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->set_transformation (self, mat);
}

gboolean
xrd_window_get_transformation (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->get_transformation (self, mat);
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
xrd_window_get_current_ppm (XrdWindow *self)
{
  guint texture_width;
  gfloat width_meters;
  gfloat scale;

  g_object_get (self,
                "texture-width", &texture_width,
                "initial-width-meters", &width_meters,
                "scale", &scale,
                NULL);

  return (float) texture_width / (width_meters * scale);
}

float
xrd_window_get_initial_ppm (XrdWindow *self)
{
  guint texture_width;
  gfloat width_meters;

  g_object_get (self,
                "texture-width", &texture_width,
                "initial-width-meters", &width_meters,
                NULL);

  return (float) texture_width / width_meters;
}

/**
 * xrd_window_get_current_width_meters:
 * @self: The #XrdWindow
 * @meters: The current width of the #XrdWindow in meters.
 *
 * Returns: The current world space width of the window in meters.
 */
float
xrd_window_get_current_width_meters (XrdWindow *self)
{
  float initial_width_meters;
  float scale;

  g_object_get (self,
                "scale", &scale,
                "initial-width-meters", &initial_width_meters,
                NULL);

  return initial_width_meters * scale;
}

/**
 * xrd_window_get_current_height_meters:
 * @self: The #XrdWindow
 * @meters: The current height of the #XrdWindow in meter.
 *
 * Returns: The current world space height of the window in meter.
 */
float
xrd_window_get_current_height_meters (XrdWindow *self)
{
  float initial_height_meters;
  float scale;

  g_object_get (self,
                "scale", &scale,
                "initial-height-meters", &initial_height_meters,
                NULL);

  return initial_height_meters * scale;
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

void
xrd_window_set_hidden (XrdWindow *self,
                       gboolean hidden)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->set_hidden (self, hidden);
}

gboolean
xrd_window_get_hidden (XrdWindow *self)
{
  XrdWindowInterface* iface = XRD_WINDOW_GET_IFACE (self);
  return iface->get_hidden (self);
}
