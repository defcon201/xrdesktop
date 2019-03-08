/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-window.h"

G_DEFINE_TYPE (XrdWindow, xrd_window, G_TYPE_OBJECT)

enum
{
  XRD_WINDOW_PROP_WINDOW_TITLE = 1,
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
    case XRD_WINDOW_PROP_WINDOW_TITLE:
      if (self->window_title)
        g_string_free (self->window_title, TRUE);
      self->window_title = g_string_new (g_value_get_string (value));
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
    case XRD_WINDOW_PROP_WINDOW_TITLE:
      g_value_set_string (value, self->window_title->str);
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


  object_class->set_property = xrd_window_set_property;
  object_class->get_property = xrd_window_get_property;

  obj_properties[XRD_WINDOW_PROP_WINDOW_TITLE] =
      g_param_spec_string ("window-title",
                           "Window Title",
                           "Title of the Window.",
                           NULL  /* default value */,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,
                                     XRD_WINDOW_N_PROPERTIES,
                                     obj_properties);

  klass->windows_created = 0;
}

gboolean
xrd_window_set_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_set_transformation_matrix == NULL)
      return FALSE;
  return klass->xrd_window_set_transformation_matrix (self, mat);
}

gboolean
xrd_window_get_transformation_matrix (XrdWindow *self, graphene_matrix_t *mat)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_transformation_matrix == NULL)
      return FALSE;
  return klass->xrd_window_get_transformation_matrix (self, mat);
}

void
xrd_window_submit_texture (XrdWindow *self,
                           GulkanClient *client,
                           GulkanTexture *texture)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_submit_texture == NULL)
      return;
  return klass->xrd_window_submit_texture (self, client, texture);
}

float
xrd_window_pixel_to_xr_scale (XrdWindow *self, int pixel)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_pixel_to_xr_scale == NULL)
    {
      return (float)pixel / self->ppm * self->scaling_factor;
    }
  return klass->xrd_window_pixel_to_xr_scale (self, pixel);
}

gboolean
xrd_window_get_xr_width (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_xr_width == NULL)
    {
      *meters = xrd_window_pixel_to_xr_scale (self, self->texture_width);
      return FALSE;
    }
  return klass->xrd_window_get_xr_width (self, meters);
}


gboolean
xrd_window_get_xr_height (XrdWindow *self, float *meters)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_xr_height == NULL)
    {
      *meters = xrd_window_pixel_to_xr_scale (self, self->texture_height);
      return TRUE;
    }
  return klass->xrd_window_get_xr_height (self, meters);
}

gboolean
xrd_window_get_scaling_factor (XrdWindow *self, float *factor)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_get_scaling_factor == NULL)
    {
      *factor = self->scaling_factor;
      return TRUE;
    }
  return klass->xrd_window_get_scaling_factor (self, factor);
}

gboolean
xrd_window_set_scaling_factor (XrdWindow *self, float factor)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_set_scaling_factor == NULL)
      return FALSE;
  return klass->xrd_window_set_scaling_factor (self, factor);
}

void
xrd_window_poll_event (XrdWindow *self)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_poll_event == NULL)
      return;
  return klass->xrd_window_poll_event (self);
}

gboolean
xrd_window_intersects (XrdWindow   *self,
                       graphene_matrix_t  *pointer_transformation_matrix,
                       graphene_point3d_t *intersection_point)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersects == NULL)
      return FALSE;
  return klass->xrd_window_intersects (self,
                                       pointer_transformation_matrix,
                                       intersection_point);
}

gboolean
xrd_window_intersection_to_window_coords (XrdWindow   *self,
                                          graphene_point3d_t *intersection_point,
                                          XrdPixelSize       *size_pixels,
                                          graphene_point_t   *window_coords)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersection_to_window_coords == NULL)
      return FALSE;
  return klass->xrd_window_intersection_to_window_coords (self,
                                                          intersection_point,
                                                          size_pixels,
                                                          window_coords);
}

gboolean
xrd_window_intersection_to_offset_center (XrdWindow *self,
                                          graphene_point3d_t *intersection_point,
                                          graphene_point_t   *offset_center)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_intersection_to_offset_center == NULL)
      return FALSE;
  return klass->xrd_window_intersection_to_offset_center (self,
                                                          intersection_point,
                                                          offset_center);
}


void
xrd_window_emit_grab_start (XrdWindow *self,
                            XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_grab_start == NULL)
      return;
  return klass->xrd_window_emit_grab_start (self, event);
}


void
xrd_window_emit_grab (XrdWindow *self,
                      XrdGrabEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_grab == NULL)
      return;
  return klass->xrd_window_emit_grab (self, event);
}

void
xrd_window_emit_release (XrdWindow *self,
                         XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_release == NULL)
      return;
  return klass->xrd_window_emit_release (self, event);
}

void
xrd_window_emit_hover_end (XrdWindow *self,
                           XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover_end == NULL)
      return;
  return klass->xrd_window_emit_hover_end (self, event);
}


void
xrd_window_emit_hover (XrdWindow    *self,
                       XrdHoverEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover == NULL)
      return;
  return klass->xrd_window_emit_hover (self, event);
}

void
xrd_window_emit_hover_start (XrdWindow *self,
                             XrdControllerIndexEvent *event)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_emit_hover_start == NULL)
      return;
  return klass->xrd_window_emit_hover_start (self, event);
}

void
xrd_window_add_child (XrdWindow *self,
                      XrdWindow *child,
                      graphene_point_t *offset_center)
{
  XrdWindowClass *klass = XRD_WINDOW_GET_CLASS (self);
  if (klass->xrd_window_add_child == NULL)
      return;
  return klass->xrd_window_add_child (self, child, offset_center);
}

static void
xrd_window_constructed (GObject *gobject)
{
  XrdWindow *self = XRD_WINDOW (gobject);

  self->scaling_factor = 1.0;
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
