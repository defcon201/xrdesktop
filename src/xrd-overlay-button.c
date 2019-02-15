/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-button.h"
#include "openvr-overlay.h"

G_DEFINE_TYPE (XrdOverlayButton, xrd_overlay_button, XRD_TYPE_OVERLAY_WINDOW)

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
xrd_overlay_button_new (gchar *text)
{
  XrdOverlayButton *self = (XrdOverlayButton*) g_object_new (XRD_TYPE_OVERLAY_BUTTON, 0);

  XrdOverlayWindow *window = XRD_OVERLAY_WINDOW (self);

  int width = 200;
  int height = 200;

  window->texture_width = width;
  window->texture_height = height;
  window->window_title = g_string_new (text);

  window->native = NULL,
  window->texture = NULL;
  window->gl_texture = 0;

  xrd_overlay_window_internal_init (window);

  unsigned char image[4 * width * height];
  cairo_surface_t* surface = _create_cairo_surface (image, width, height, text);

  if (surface == NULL) {
    g_printerr ("Could not create cairo surface.\n");
    return NULL;
  }

  openvr_overlay_set_cairo_surface_raw (window->overlay, surface);
  cairo_surface_destroy (surface);

  return self;
}

static void
xrd_overlay_button_finalize (GObject *gobject)
{
  XrdOverlayButton *self = XRD_OVERLAY_BUTTON (gobject);
  (void) self;
}

void
xrd_overlay_button_unmark (XrdOverlayButton *self)
{
  graphene_vec3_t unmarked_color;
  graphene_vec3_init (&unmarked_color, 1.f, 1.f, 1.f);
  OpenVROverlay *overlay = XRD_OVERLAY_WINDOW (self)->overlay;
  openvr_overlay_set_color (overlay, &unmarked_color);
}

void
xrd_overlay_button_mark_color (XrdOverlayButton *self,
                               float r, float g, float b)
{
  graphene_vec3_t marked_color;
  //graphene_vec3_init (&marked_color, .8f, .4f, .2f);
  graphene_vec3_init (&marked_color, r, g, b);
  OpenVROverlay *overlay = XRD_OVERLAY_WINDOW (self)->overlay;
  openvr_overlay_set_color (overlay, &marked_color);
}

