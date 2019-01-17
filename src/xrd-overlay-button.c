/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-button.h"

G_DEFINE_TYPE (XrdOverlayButton, xrd_overlay_button, OPENVR_TYPE_OVERLAY)

static void
xrd_overlay_button_finalize (GObject *gobject);

static void
xrd_overlay_button_class_init (XrdOverlayButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_button_finalize;
}

static void
xrd_overlay_button_init (XrdOverlayButton *self)
{
  (void) self;
}

cairo_surface_t*
_create_cairo_surface (unsigned char *image, uint32_t width,
                       uint32_t height, const gchar *text)
{
  cairo_surface_t *surface =
    cairo_image_surface_create_for_data (image,
                                         CAIRO_FORMAT_ARGB32,
                                         width, height,
                                         width * 4);

  cairo_t *cr = cairo_create (surface);

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill (cr);

  double r0;
  if (width < height)
    r0 = (double) width / 3.0;
  else
    r0 = (double) height / 3.0;

  double radius = r0 * 3.0;
  double r1 = r0 * 5.0;

  double center_x = (double) width / 2.0;
  double center_y = (double) height / 2.0;

  double cx0 = center_x - r0 / 2.0;
  double cy0 = center_y - r0;
  double cx1 = center_x - r0;
  double cy1 = center_y - r0;

  cairo_pattern_t *pat = cairo_pattern_create_radial (cx0, cy0, r0,
                                                      cx1, cy1, r1);
  cairo_pattern_add_color_stop_rgba (pat, 0, .3, .3, .3, 1);
  cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
  cairo_set_source (cr, pat);
  cairo_arc (cr, center_x, center_y, radius, 0, 2 * M_PI);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);

  cairo_select_font_face (cr, "Sans",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_NORMAL);

  cairo_set_font_size (cr, 52.0);

  cairo_text_extents_t extents;
  cairo_text_extents (cr, text, &extents);

  cairo_move_to (cr,
                 center_x - extents.width / 2,
                 center_y  - extents.height / 2);
  cairo_set_source_rgb (cr, 0.9, 0.9, 0.9);
  cairo_show_text (cr, text);

  cairo_destroy (cr);

  return surface;
}

XrdOverlayButton *
xrd_overlay_button_new (gchar *id, gchar *text)
{
  XrdOverlayButton *self = (XrdOverlayButton*) g_object_new (XRD_TYPE_OVERLAY_BUTTON, 0);

  /* create openvr overlay */
  openvr_overlay_create (OPENVR_OVERLAY (self), id, text);

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
  {
    g_printerr ("Overlay unavailable.\n");
    return NULL;
  }

  guint width = 200;
  guint height = 200;
  unsigned char image[4 * width * height];
  cairo_surface_t* surface = _create_cairo_surface (image, width, height, text);

  if (surface == NULL) {
    g_printerr ("Could not create cairo surface.\n");
    return NULL;
  }

  openvr_overlay_set_cairo_surface_raw (OPENVR_OVERLAY (self), surface);
  cairo_surface_destroy (surface);

  if (!openvr_overlay_show (OPENVR_OVERLAY (self)))
    return NULL;

  return self;
}

static void
xrd_overlay_button_finalize (GObject *gobject)
{
  XrdOverlayButton *self = XRD_OVERLAY_BUTTON (gobject);
  (void) self;
}
