/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-window.h"
#include <gdk/gdk.h>

G_DEFINE_TYPE (XrdWindow, xrd_window, G_TYPE_OBJECT)

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

enum
{
  XRD_WINDOW_PROP_TITLE = 1,
  XRD_WINDOW_PROP_PPM,
  XRD_WINDOW_PROP_SCALING,
  XRD_WINDOW_PROP_NATIVE,
  XRD_WINDOW_N_PROPERTIES
};

static GParamSpec *obj_properties[XRD_WINDOW_N_PROPERTIES] = { NULL, };

static void
xrd_window_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  XrdWindow *self = XRD_WINDOW (object);
  switch (property_id)
    {
    case XRD_WINDOW_PROP_TITLE:
      if (self->window_title)
        g_string_free (self->window_title, TRUE);
      self->window_title = g_string_new (g_value_get_string (value));
      break;
    case XRD_WINDOW_PROP_PPM:
      self->ppm = g_value_get_float (value);
      break;
    case XRD_WINDOW_PROP_SCALING:
      self->scaling_factor = g_value_get_float (value);
      break;
    case XRD_WINDOW_PROP_NATIVE:
      self->native = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xrd_window_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  XrdWindow *self = XRD_WINDOW (object);

  switch (property_id)
    {
    case XRD_WINDOW_PROP_TITLE:
      g_value_set_string (value, self->window_title->str);
      break;
    case XRD_WINDOW_PROP_PPM:
      g_value_set_float (value, self->ppm);
      break;
    case XRD_WINDOW_PROP_SCALING:
      g_value_set_float (value, self->scaling_factor);
      break;
    case XRD_WINDOW_PROP_NATIVE:
      g_value_set_pointer (value, self->native);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xrd_window_finalize (GObject *gobject);
static void
xrd_window_constructed (GObject *gobject);

static void
xrd_window_class_init (XrdWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_window_finalize;
  object_class->constructed = xrd_window_constructed;


  window_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new ("motion-notify-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[SHOW] =
    g_signal_new ("show",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DESTROY] =
    g_signal_new ("destroy",
                   G_TYPE_FROM_CLASS (klass),
                     G_SIGNAL_RUN_CLEANUP |
                      G_SIGNAL_NO_RECURSE |
                      G_SIGNAL_NO_HOOKS,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[SCROLL_EVENT] =
    g_signal_new ("scroll-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[KEYBOARD_CLOSE_EVENT] =
    g_signal_new ("keyboard-close-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[GRAB_START_EVENT] =
    g_signal_new ("grab-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[GRAB_EVENT] =
    g_signal_new ("grab-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  window_signals[RELEASE_EVENT] =
    g_signal_new ("release-event",
                  G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_END_EVENT] =
    g_signal_new ("hover-end-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_EVENT] =
    g_signal_new ("hover-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  window_signals[HOVER_START_EVENT] =
    g_signal_new ("hover-start-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);


  object_class->set_property = xrd_window_set_property;
  object_class->get_property = xrd_window_get_property;

  obj_properties[XRD_WINDOW_PROP_TITLE] =
      g_param_spec_string ("window-title",
                           "Window Title",
                           "Title of the Window.",
                           NULL  /* default value */,
                           /* TODO: changeable window description
                            * can not change overlay key? */
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  obj_properties[XRD_WINDOW_PROP_PPM] =
    g_param_spec_float ("ppm",
                       "Pixels Per Meter",
                       "Pixels Per Meter Setting of this Window.",
                       0.  /* minimum value */,
                       16384. /* maximum value */,
                       450.  /* default value */,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  obj_properties[XRD_WINDOW_PROP_SCALING] =
    g_param_spec_float ("scaling-factor",
                       "Scaling Factor",
                       "Scaling Factor of this Window.",
                        /* TODO: use gsettings values */
                       .1  /* minimum value */,
                       10. /* maximum value */,
                       1.  /* default value */,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  obj_properties[XRD_WINDOW_PROP_NATIVE] =
    g_param_spec_pointer ("native",
                          "Native Window Handle",
                          "A pointer to an (opaque) native window struct.",
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,
                                     XRD_WINDOW_N_PROPERTIES,
                                     obj_properties);

  klass->windows_created = 0;
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->set_transformation_matrix == NULL)
      return FALSE;
  return klass->set_transformation_matrix (self, mat);
}

gboolean
xrd_window_get_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->get_transformation_matrix == NULL)
      return FALSE;
  return klass->get_transformation_matrix (self, mat);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->submit_texture == NULL)
      return;
  return klass->submit_texture (self, client, texture);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->pixel_to_meter == NULL)
    {
      return (float)pixel / self->ppm * self->scaling_factor;
    }
  return klass->pixel_to_meter (self, pixel);
}

/**
 * xrd_window_get_width_meter:
 * @self: The #XrdWindow
 *
 * Returns: The current world space width of the window in meter.
 */
gboolean
xrd_window_get_width_meter (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->get_width_meter == NULL)
    {
      *meters = xrd_window_pixel_to_meter (self, self->texture_width);
      return FALSE;
    }
  return klass->get_width_meter (self, meters);
}

/**
 * xrd_window_get_height_meter:
 * @self: The #XrdWindow
 *
 * Returns: The current world space height of the window in meter.
 */
gboolean
xrd_window_get_height_meter (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->get_height_meter == NULL)
    {
      *meters = xrd_window_pixel_to_meter (self, self->texture_height);
      return TRUE;
    }
  return klass->get_height_meter (self, meters);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->poll_event == NULL)
      return;
  return klass->poll_event (self);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->intersects == NULL)
      return FALSE;
  return klass->intersects (self,
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->intersection_to_pixels == NULL)
      return FALSE;
  return klass->intersection_to_pixels (self,
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->intersection_to_2d_offset_meter == NULL)
      return FALSE;
  return klass->intersection_to_2d_offset_meter (self,
                                                 intersection_point,
                                                 offset_center);
}

void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_grab_start == NULL)
    g_signal_emit (self, window_signals[GRAB_START_EVENT], 0, event);
  else
    return klass->emit_grab_start (self, event);
}


void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_grab == NULL)
    g_signal_emit (self, window_signals[GRAB_EVENT], 0, event);
  else
    return klass->emit_grab (self, event);
}

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_release == NULL)
    g_signal_emit (self, window_signals[RELEASE_EVENT], 0, event);
  else
    return klass->emit_release (self, event);
}

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_hover_end == NULL)
    g_signal_emit (self, window_signals[HOVER_END_EVENT], 0, event);
  else
    return klass->emit_hover_end (self, event);
}


void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_hover == NULL)
    g_signal_emit (self, window_signals[HOVER_EVENT], 0, event);
  else
    return klass->emit_hover (self, event);
}

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->emit_hover_start == NULL)
    g_signal_emit (self, window_signals[HOVER_START_EVENT], 0, event);
  else
    return klass->emit_hover_start (self, event);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->add_child == NULL)
      return;
  return klass->add_child (self, child, offset_center);
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
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->set_color == NULL)
      return;
  return klass->set_color (self, color);
}

static void
xrd_window_constructed (GObject *gobject)
{
  XrdWindow *self = XRD_WINDOW (gobject);

  self->child_window = NULL;
  self->parent_window = NULL;
  self->texture_width = 0;
  self->texture_height = 0;

  G_OBJECT_CLASS (xrd_window_parent_class)->constructed (gobject);
}

void
xrd_window_init (XrdWindow *self)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  (void) klass;
}

static void
xrd_window_finalize (GObject *gobject)
{
  XrdWindow *self = XRD_WINDOW (gobject);
  (void) self;
  G_OBJECT_CLASS (xrd_window_parent_class)->finalize (gobject);
}
