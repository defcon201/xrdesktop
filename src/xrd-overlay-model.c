/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-model.h"
#include "openvr-context.h"

G_DEFINE_TYPE (XrdOverlayModel, xrd_overlay_model, OPENVR_TYPE_OVERLAY)

static void
xrd_overlay_model_finalize (GObject *gobject);

static void
xrd_overlay_model_class_init (XrdOverlayModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_model_finalize;
}

static void
xrd_overlay_model_init (XrdOverlayModel *self)
{
  (void) self;
}

void
_destroy_pixels_cb (guchar *pixels, gpointer unused)
{
  (void) unused;
  g_free (pixels);
}

GdkPixbuf *
_create_empty_pixbuf (uint32_t width, uint32_t height)
{
  guchar *pixels = (guchar*) g_malloc (sizeof (guchar) * height * width * 4);
  memset (pixels, 0, height * width * 4 * sizeof (guchar));
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
                                                TRUE, 8, width, height,
                                                4 * width,
                                                _destroy_pixels_cb, NULL);
  return pixbuf;
}

XrdOverlayModel *
xrd_overlay_model_new (gchar* key, gchar* name)
{
  XrdOverlayModel *self = (XrdOverlayModel*) g_object_new (XRD_TYPE_OVERLAY_MODEL, 0);

  if (!xrd_overlay_model_initialize (self, key, name))
    return NULL;

  return self;
}

/*
 * TODO: Not sure how the parent _new constructor can be shared with
 *       XrdOverlayPointer. Casting to the child class is not allowed.
 *       As a workaround I am introducting this initialization method.
 */

gboolean
xrd_overlay_model_initialize (XrdOverlayModel *self, gchar* key, gchar* name)
{
  OpenVROverlay *overlay = OPENVR_OVERLAY (self);

  if (!openvr_overlay_create (overlay, key, name))
    return FALSE;

  if (!openvr_overlay_is_valid (overlay))
    {
      g_printerr ("Model overlay %s %s unavailable.\n", key, name);
      return FALSE;
    }

  GdkPixbuf *pixbuf = _create_empty_pixbuf (10, 10);
  if (pixbuf == NULL)
    return FALSE;

  /*
   * Overlay needs a texture to be set to show model
   * See https://github.com/ValveSoftware/openvr/issues/496
   */
  openvr_overlay_set_gdk_pixbuf_raw (overlay, pixbuf);
  g_object_unref (pixbuf);

  if (!openvr_overlay_set_alpha (overlay, 0.0f))
    return FALSE;

  return TRUE;
}

static void
xrd_overlay_model_finalize (GObject *gobject)
{
  XrdOverlayModel *self = XRD_OVERLAY_MODEL (gobject);
  (void) self;
}

/*
 * Sets render model to draw behind this overlay and the vertex color to use,
 * pass NULL for color to match the overlays vertex color
 */
gboolean
xrd_overlay_model_set_model (XrdOverlayModel *self, gchar *name,
                          struct HmdColor_t *color)
{
  GET_OVERLAY_FUNCTIONS

  err = f->SetOverlayRenderModel (OPENVR_OVERLAY (self)->overlay_handle,
                                  name, color);

  OVERLAY_CHECK_ERROR ("SetOverlayRenderModel", err);
  return TRUE;
}

gboolean
xrd_overlay_model_get_model (XrdOverlayModel *self, gchar *name,
                        struct HmdColor_t *color, uint32_t *id)
{
  GET_OVERLAY_FUNCTIONS

  *id = f->GetOverlayRenderModel (OPENVR_OVERLAY (self)->overlay_handle,
                                  name, k_unMaxPropertyStringSize,
                                  color, &err);

  OVERLAY_CHECK_ERROR ("GetOverlayRenderModel", err);
  return TRUE;
}

