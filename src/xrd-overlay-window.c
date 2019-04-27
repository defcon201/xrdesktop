/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"

#include <glib/gprintf.h>

#include <openvr-overlay-uploader.h>
#include "xrd-math.h"
#include "graphene-ext.h"

struct _XrdOverlayWindow
{
  OpenVROverlay parent;
  gboolean      recreate;
  gboolean      flip_y;
  gboolean       hidden;

  XrdWindowData window_data;
};

enum
{
  PROP_TITLE = 1,
  PROP_PPM,
  PROP_SCALING,
  PROP_NATIVE,
  PROP_TEXTURE_WIDTH,
  PROP_TEXTURE_HEIGHT,
  N_PROPERTIES
};

static void
xrd_overlay_window_window_interface_init (XrdWindowInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayWindow, xrd_overlay_window, OPENVR_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_WINDOW,
                                                xrd_overlay_window_window_interface_init))

static struct VRTextureBounds_t defaultBounds = { 0., 0., 1., 1. };
static struct VRTextureBounds_t flippedBounds = { 0., 1., 1., 0. };

static void
_scale_move_child (XrdOverlayWindow *self);


static void
xrd_overlay_window_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (object);
  switch (property_id)
    {
    case PROP_TITLE:
      if (self->window_data.window_title)
        g_string_free (self->window_data.window_title, TRUE);
      self->window_data.window_title = g_string_new (g_value_get_string (value));
      break;
    case PROP_PPM:
      self->window_data.ppm = g_value_get_float (value);
      break;
    case PROP_SCALING:
      self->window_data.scaling_factor = g_value_get_float (value);
      break;
    case PROP_NATIVE:
      self->window_data.native = g_value_get_pointer (value);
      break;
    case PROP_TEXTURE_WIDTH:
      self->window_data.texture_width = g_value_get_uint (value);
      break;
    case PROP_TEXTURE_HEIGHT:
      self->window_data.texture_height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xrd_overlay_window_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (object);

  switch (property_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->window_data.window_title->str);
      break;
    case PROP_PPM:
      g_value_set_float (value, self->window_data.ppm);
      break;
    case PROP_SCALING:
      g_value_set_float (value, self->window_data.scaling_factor);
      break;
    case PROP_NATIVE:
      g_value_set_pointer (value, self->window_data.native);
      break;
    case PROP_TEXTURE_WIDTH:
      g_value_set_uint (value, self->window_data.texture_width);
      break;
    case PROP_TEXTURE_HEIGHT:
      g_value_set_uint (value, self->window_data.texture_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
notify_property_scale_changed (GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
  (void) pspec;
  (void) user_data;

  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (object);
  XrdWindow *xrd_window = XRD_WINDOW (object);

  float width_meter =
      xrd_window_pixel_to_meter (xrd_window, self->window_data.texture_width);

  openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), width_meter);

  if (self->window_data.child_window)
    _scale_move_child (self);
}

static void
xrd_overlay_window_finalize (GObject *gobject);

static void
xrd_overlay_window_constructed (GObject *gobject);

static void
xrd_overlay_window_class_init (XrdOverlayWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = xrd_overlay_window_finalize;
  object_class->constructed = xrd_overlay_window_constructed;

  object_class->set_property = xrd_overlay_window_set_property;
  object_class->get_property = xrd_overlay_window_get_property;

  g_object_class_override_property (object_class, PROP_TITLE, "window-title");
  g_object_class_override_property (object_class, PROP_PPM, "ppm");
  g_object_class_override_property (object_class, PROP_SCALING, "scaling-factor");
  g_object_class_override_property (object_class, PROP_NATIVE, "native");
  g_object_class_override_property (object_class, PROP_TEXTURE_WIDTH, "texture-width");
  g_object_class_override_property (object_class, PROP_TEXTURE_HEIGHT, "texture-height");
}

static void
xrd_overlay_window_window_interface_init (XrdWindowInterface *iface)
{
  iface->set_transformation_matrix =
      (void*)xrd_overlay_window_set_transformation_matrix;
  iface->get_transformation_matrix =
      (void*)xrd_overlay_window_get_transformation_matrix;
  iface->submit_texture = (void*)xrd_overlay_window_submit_texture;
  iface->poll_event = (void*)xrd_overlay_window_poll_event;
  iface->intersects = (void*)xrd_overlay_window_intersects;
  iface->intersection_to_pixels =
      (void*)xrd_overlay_window_intersection_to_pixels;
  iface->intersection_to_2d_offset_meter =
      (void*)xrd_overlay_window_intersection_to_2d_offset_meter;
  iface->add_child = (void*)xrd_overlay_window_add_child;
  iface->set_color = (void*)xrd_overlay_window_set_color;
  iface->set_flip_y = (void*)xrd_overlay_window_set_flip_y;
  iface->set_hidden = (void*)xrd_overlay_window_set_hidden;
  iface->get_hidden = (void*)xrd_overlay_window_get_hidden;
}

static void
_scale_move_child (XrdOverlayWindow *self)
{
  XrdOverlayWindow *child = XRD_OVERLAY_WINDOW (self->window_data.child_window);

  g_object_set (G_OBJECT(child), "scaling-factor", self->window_data.scaling_factor, NULL);

  graphene_point_t scaled_offset;
  graphene_point_scale (&self->window_data.child_offset_center,
                        self->window_data.scaling_factor / self->window_data.ppm,
                        &scaled_offset);

  graphene_point3d_t scaled_offset3d = {
    .x = scaled_offset.x,
    .y = scaled_offset.y,
    .z = 0.01
  };
  graphene_matrix_t child_transform;
  graphene_matrix_init_translate (&child_transform, &scaled_offset3d);

  graphene_matrix_t parent_transform;
  xrd_overlay_window_get_transformation_matrix (self, &parent_transform);

  graphene_matrix_multiply (&child_transform, &parent_transform,
                            &child_transform);

  xrd_overlay_window_set_transformation_matrix (child, &child_transform);

}

gboolean
xrd_overlay_window_set_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  gboolean res =
    openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), mat);
  if (self->window_data.child_window)
    _scale_move_child (self);
  return res;
}

gboolean
xrd_overlay_window_get_transformation_matrix (XrdOverlayWindow *self,
                                              graphene_matrix_t *mat)
{
  return openvr_overlay_get_transform_absolute (OPENVR_OVERLAY (self), mat);
}

void
xrd_overlay_window_submit_texture (XrdOverlayWindow *self,
                                   GulkanClient *client,
                                   GulkanTexture *texture)
{
  XrdWindow *xrd_window = XRD_WINDOW (self);

  OpenVROverlayUploader *uploader = OPENVR_OVERLAY_UPLOADER (client);

  if (self->window_data.texture_width != texture->width ||
      self->window_data.texture_height != texture->height)
    {
      float new_width_meter =
        xrd_window_pixel_to_meter (xrd_window, texture->width);

      openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), new_width_meter);

      self->window_data.texture_width = texture->width;
      self->window_data.texture_height = texture->height;
      /* Mouse scale is required for the intersection test */
      openvr_overlay_set_mouse_scale (OPENVR_OVERLAY (self),
                                      self->window_data.texture_width,
                                      self->window_data.texture_height);
    }

  openvr_overlay_uploader_submit_frame(uploader,
                                       OPENVR_OVERLAY (self), texture);
}

void
xrd_overlay_window_add_child (XrdOverlayWindow *self,
                              XrdOverlayWindow *child,
                              graphene_point_t *offset_center)
{
  self->window_data.child_window = XRD_WINDOW (child);
  graphene_point_init_from_point (&self->window_data.child_offset_center,
                                  offset_center);

  if (child)
    {
      _scale_move_child (self);
      XRD_OVERLAY_WINDOW (child)->window_data.parent_window = XRD_WINDOW (self);
      /* TODO: sort order hierarchy instead od ad hoc values*/
      openvr_overlay_set_sort_order (OPENVR_OVERLAY (child), 1);
    }
}

void
xrd_overlay_window_poll_event (XrdOverlayWindow *self)
{
  openvr_overlay_poll_event (OPENVR_OVERLAY (self));
}

gboolean
xrd_overlay_window_intersects (XrdOverlayWindow   *self,
                               graphene_matrix_t  *pointer_transformation_matrix,
                               graphene_point3d_t *intersection_point)
{
  gboolean res = openvr_overlay_intersects (OPENVR_OVERLAY (self),
                                            intersection_point,
                                            pointer_transformation_matrix);
  return res;
}

gboolean
xrd_overlay_window_intersection_to_pixels (XrdOverlayWindow   *self,
                                           graphene_point3d_t *intersection_point,
                                           XrdPixelSize       *size_pixels,
                                           graphene_point_t   *window_coords)
{
  PixelSize pix_size = {
    .width = size_pixels->width,
    .height = size_pixels->height
  };
  gboolean res =
      openvr_overlay_get_2d_intersection (OPENVR_OVERLAY (self),
                                          intersection_point,
                                          &pix_size, window_coords);
  return res;
}

gboolean
xrd_overlay_window_intersection_to_2d_offset_meter (XrdOverlayWindow *self,
                                                    graphene_point3d_t *intersection_point,
                                                    graphene_point_t   *offset_center)
{
  gboolean res =
      openvr_overlay_get_2d_offset (OPENVR_OVERLAY (self),
                                    intersection_point, offset_center);
  return res;
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
  self->flip_y = false;
  self->hidden = false;
  self->window_data.child_window = NULL;
  self->window_data.parent_window = NULL;
  self->window_data.native = NULL;
  self->window_data.texture_width = 0;
  self->window_data.texture_height = 0;
}

/** xrd_overlay_window_new:
 * Create a new XrdWindow. Note that the window will only have dimensions after
 * a texture is uploaded. */
XrdOverlayWindow *
xrd_overlay_window_new (gchar *window_title, float ppm, gpointer native)
{
  XrdOverlayWindow *self =
      (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW,
                                        "window-title", window_title,
                                        "ppm", ppm,
                                        "native", native,
                                         NULL);
  return self;
}

void
xrd_overlay_window_set_color (XrdOverlayWindow *self,
                              graphene_vec3_t *color)
{
  openvr_overlay_set_color (OPENVR_OVERLAY (self), color);
}

void
xrd_overlay_window_set_flip_y (XrdOverlayWindow *self,
                               gboolean flip_y)
{
  if (flip_y != self->flip_y)
    {
      OpenVRContext *openvrContext = openvr_context_get_instance();
      VRTextureBounds_t *bounds = flip_y ? &flippedBounds : &defaultBounds;
      openvrContext->overlay->SetOverlayTextureBounds (
          OPENVR_OVERLAY (self)->overlay_handle, bounds);

      self->flip_y = flip_y;
    }
}

void
xrd_overlay_window_set_hidden (XrdOverlayWindow *self,
                               gboolean hidden)
{
  if (self->hidden == hidden)
    return;

  self->hidden = hidden;
  if (hidden)
    openvr_overlay_hide (OPENVR_OVERLAY (self));
  else
    openvr_overlay_show (OPENVR_OVERLAY (self));
}

gboolean
xrd_overlay_window_get_hidden (XrdOverlayWindow *self)
{
  return self->hidden;
}

static void
xrd_overlay_window_constructed (GObject *gobject)
{
  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->constructed (gobject);

  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);
  XrdWindow *xrd_window = XRD_WINDOW (self);
  XrdWindowInterface *iface = XRD_WINDOW_GET_IFACE (xrd_window);

  gchar overlay_id_str [25];
  g_sprintf (overlay_id_str, "xrd-window-%d", iface->windows_created);

  openvr_overlay_create (OPENVR_OVERLAY (self), overlay_id_str,
                         self->window_data.window_title->str);

  /* g_print ("Created overlay %s\n", overlay_id_str); */

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
  {
    g_printerr ("Overlay unavailable.\n");
    return;
  }

  openvr_overlay_show (OPENVR_OVERLAY (self));

  iface->windows_created++;

  g_signal_connect(xrd_window, "notify::scaling-factor",
                   (GCallback)notify_property_scale_changed, NULL);

  g_signal_connect(xrd_window, "notify::ppm",
                   (GCallback)notify_property_scale_changed, NULL);
}

static void
xrd_overlay_window_finalize (GObject *gobject)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (gobject);

  XrdOverlayWindow *parent =
    XRD_OVERLAY_WINDOW (self->window_data.parent_window);

  if (parent != NULL)
    parent->window_data.child_window = NULL;

  /* TODO: a child window should not exist without a parent window anyway,
   * but it will be cleaned up already because the child window on the desktop
   * will most likely close already. */

  XrdOverlayWindow *child = XRD_OVERLAY_WINDOW (self->window_data.child_window);
  if (child)
    child->window_data.parent_window = NULL;

  G_OBJECT_CLASS (xrd_overlay_window_parent_class)->finalize (gobject);
}
