/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-window.h"

#include <glib/gprintf.h>

#include <openvr-glib.h>
#include "xrd-math.h"
#include "graphene-ext.h"

struct _XrdOverlayWindow
{
  OpenVROverlay parent;
  gboolean      recreate;
  gboolean      hidden;

  XrdWindowData window_data;
};

enum
{
  PROP_TITLE = 1,
  PROP_SCALE,
  PROP_NATIVE,
  PROP_TEXTURE_WIDTH,
  PROP_TEXTURE_HEIGHT,
  PROP_WIDTH_METERS,
  PROP_HEIGHT_METERS,
  N_PROPERTIES
};

static void
xrd_overlay_window_window_interface_init (XrdWindowInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XrdOverlayWindow, xrd_overlay_window, OPENVR_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (XRD_TYPE_WINDOW,
                                                xrd_overlay_window_window_interface_init))

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
      if (self->window_data.title)
        g_string_free (self->window_data.title, TRUE);
      self->window_data.title = g_string_new (g_value_get_string (value));
      break;
    case PROP_SCALE:
      self->window_data.scale = g_value_get_float (value);
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
    case PROP_WIDTH_METERS:
      self->window_data.initial_size_meters.x = g_value_get_float (value);
      break;
    case PROP_HEIGHT_METERS:
      self->window_data.initial_size_meters.y = g_value_get_float (value);
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
      g_value_set_string (value, self->window_data.title->str);
      break;
    case PROP_SCALE:
      g_value_set_float (value, self->window_data.scale);
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
    case PROP_WIDTH_METERS:
      g_value_set_float (value, self->window_data.initial_size_meters.x);
      break;
    case PROP_HEIGHT_METERS:
      g_value_set_float (value, self->window_data.initial_size_meters.y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
_update_dimensions (XrdOverlayWindow *self)
{
  float width_meters = xrd_window_get_current_width_meters (XRD_WINDOW (self));
  openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), width_meters);

  uint32_t w, h;
  g_object_get (self,
                "texture-width", &w,
                "texture-height", &h,
                NULL);

  openvr_overlay_set_mouse_scale (OPENVR_OVERLAY (self), w, h);

  if (self->window_data.child_window)
    _scale_move_child (self);
}

static void
_update_dimensions_cb (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  (void) pspec;
  (void) user_data;

  _update_dimensions (XRD_OVERLAY_WINDOW (object));
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

  g_object_class_override_property (object_class, PROP_TITLE, "title");
  g_object_class_override_property (object_class, PROP_SCALE, "scale");
  g_object_class_override_property (object_class, PROP_NATIVE, "native");
  g_object_class_override_property (object_class, PROP_TEXTURE_WIDTH, "texture-width");
  g_object_class_override_property (object_class, PROP_TEXTURE_HEIGHT, "texture-height");
  g_object_class_override_property (object_class, PROP_WIDTH_METERS, "initial-width-meters");
  g_object_class_override_property (object_class, PROP_HEIGHT_METERS, "initial-height-meters");
}

static void
_scale_move_child (XrdOverlayWindow *self)
{
  XrdOverlayWindow *child = XRD_OVERLAY_WINDOW (self->window_data.child_window);

  g_object_set (G_OBJECT(child), "scale", self->window_data.scale, NULL);

  float initial_ppm = xrd_window_get_initial_ppm (XRD_WINDOW (self));

  graphene_point_t scaled_offset;
  graphene_point_scale (&self->window_data.child_offset_center,
                        self->window_data.scale / initial_ppm,
                        &scaled_offset);

  graphene_point3d_t scaled_offset3d = {
    .x = scaled_offset.x,
    .y = scaled_offset.y,
    .z = 0.01
  };
  graphene_matrix_t child_transform;
  graphene_matrix_init_translate (&child_transform, &scaled_offset3d);

  graphene_matrix_t parent_transform;
  xrd_window_get_transformation (XRD_WINDOW (self), &parent_transform);

  graphene_matrix_multiply (&child_transform, &parent_transform,
                            &child_transform);

  xrd_window_set_transformation (XRD_WINDOW (child), &child_transform);

}

static gboolean
_set_transformation (XrdWindow         *window,
                     graphene_matrix_t *mat)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  gboolean res =
    openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), mat);
  if (self->window_data.child_window)
    _scale_move_child (self);
  return res;
}

static gboolean
_get_transformation (XrdWindow         *window,
                     graphene_matrix_t *mat)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  return openvr_overlay_get_transform_absolute (OPENVR_OVERLAY (self), mat);
}

static void
_submit_texture (XrdWindow     *window,
                 GulkanClient  *client,
                 GulkanTexture *texture)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  OpenVROverlayUploader *uploader = OPENVR_OVERLAY_UPLOADER (client);

  uint32_t current_width, current_height;
  g_object_get (self,
                "texture-width", &current_width,
                "texture-height", &current_height,
                NULL);

  guint new_width = gulkan_texture_get_width (texture);
  guint new_height = gulkan_texture_get_height (texture);

  if (current_width != new_width || current_height != new_height)
    {
      g_object_set (self,
                    "texture-width", new_width,
                    "texture-height", new_height,
                    NULL);

      float width_meters =
        xrd_window_get_current_width_meters (XRD_WINDOW (self));

      openvr_overlay_set_width_meters (OPENVR_OVERLAY (self), width_meters);

      /* Mouse scale is required for the intersection test */
      openvr_overlay_set_mouse_scale (OPENVR_OVERLAY (self),
                                      new_width, new_height);
    }

  openvr_overlay_uploader_submit_frame (uploader,
                                        OPENVR_OVERLAY (self), texture);
}

static void
_add_child (XrdWindow        *window,
            XrdWindow        *child,
            graphene_point_t *offset_center)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
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

static void
_poll_event (XrdWindow *window)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  openvr_overlay_poll_event (OPENVR_OVERLAY (self));
}

static gboolean
_intersects (XrdWindow          *window,
             graphene_matrix_t  *pointer_transformation_matrix,
             graphene_point3d_t *intersection_point)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  gboolean res = openvr_overlay_intersects (OPENVR_OVERLAY (self),
                                            intersection_point,
                                            pointer_transformation_matrix);
  return res;
}

static gboolean
_intersection_to_pixels (XrdWindow          *window,
                         graphene_point3d_t *intersection_point,
                         XrdPixelSize       *size_pixels,
                         graphene_point_t   *window_coords)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
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

static gboolean
_intersection_to_2d_offset_meter (XrdWindow          *window,
                                  graphene_point3d_t *intersection_point,
                                  graphene_point_t   *offset_center)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  gboolean res =
      openvr_overlay_get_2d_offset (OPENVR_OVERLAY (self),
                                    intersection_point, offset_center);
  return res;
}

static void
xrd_overlay_window_init (XrdOverlayWindow *self)
{
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
xrd_overlay_window_new (const gchar *title)
{
  XrdOverlayWindow *self =
      (XrdOverlayWindow*) g_object_new (XRD_TYPE_OVERLAY_WINDOW,
                                        "title", title, NULL);
  return self;
}

XrdOverlayWindow *
xrd_overlay_window_new_from_meters (const gchar *title,
                                    float        width_meters,
                                    float        height_meters)
{
  XrdOverlayWindow *window = xrd_overlay_window_new (title);
  g_object_set (window,
                "initial-width-meters", width_meters,
                "initial-height-meters", height_meters,
                NULL);
  return window;
}


XrdOverlayWindow *
xrd_overlay_window_new_from_ppm (const gchar *title,
                                 uint32_t     width_pixels,
                                 uint32_t     height_pixels,
                                 float        ppm)
{
  XrdOverlayWindow *window = xrd_overlay_window_new (title);
  g_object_set (window,
                "texture-width", width_pixels,
                "texture-height", height_pixels,
                "initial-width-meters", width_pixels / ppm,
                "initial-height-meters", height_pixels / ppm,
                NULL);
  return window;
}

XrdOverlayWindow *
xrd_overlay_window_new_from_native (const gchar *title,
                                    gpointer     native,
                                    uint32_t     width_pixels,
                                    uint32_t     height_pixels,
                                    float        ppm)
{
  XrdOverlayWindow *window = xrd_overlay_window_new_from_ppm (title,
                                                              width_pixels,
                                                              height_pixels,
                                                              ppm);
  g_object_set (window, "native", native, NULL);
  return window;
}

static void
_set_color (XrdWindow       *window,
            graphene_vec3_t *color)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  openvr_overlay_set_color (OPENVR_OVERLAY (self), color);
}

static void
_set_hidden (XrdWindow *window,
             gboolean   hidden)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  if (self->hidden == hidden)
    return;

  self->hidden = hidden;
  if (hidden)
    openvr_overlay_hide (OPENVR_OVERLAY (self));
  else
    openvr_overlay_show (OPENVR_OVERLAY (self));
}

static gboolean
_get_hidden (XrdWindow *window)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
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
                         self->window_data.title->str);

  /* g_print ("Created overlay %s\n", overlay_id_str); */

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
  {
    g_printerr ("Overlay unavailable.\n");
    return;
  }

  openvr_overlay_show (OPENVR_OVERLAY (self));

  iface->windows_created++;

  g_signal_connect(xrd_window, "notify::scale",
                   (GCallback) _update_dimensions_cb, NULL);

  g_signal_connect(xrd_window, "notify::initial-width-meters",
                   (GCallback) _update_dimensions_cb, NULL);
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

static XrdWindowData*
_get_data (XrdWindow *window)
{
  XrdOverlayWindow *self = XRD_OVERLAY_WINDOW (window);
  return &self->window_data;
}

static void
xrd_overlay_window_window_interface_init (XrdWindowInterface *iface)
{
  iface->set_transformation = _set_transformation;
  iface->get_transformation = _get_transformation;
  iface->submit_texture = _submit_texture;
  iface->poll_event = _poll_event;
  iface->intersects = _intersects;
  iface->intersection_to_pixels = _intersection_to_pixels;
  iface->intersection_to_2d_offset_meter = _intersection_to_2d_offset_meter;
  iface->add_child = _add_child;
  iface->set_color = _set_color;
  iface->set_flip_y = (void*)openvr_overlay_set_flip_y;
  iface->set_hidden = _set_hidden;
  iface->get_hidden = _get_hidden;
  iface->get_data = _get_data;
}

