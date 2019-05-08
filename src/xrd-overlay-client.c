/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-client.h"

#include <gdk/gdk.h>
#include <openvr-io.h>
#include <glib/gprintf.h>
#include <openvr-math.h>
#include "xrd-math.h"
#include "xrd-client.h"
#include "graphene-ext.h"
#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"
#include "xrd-overlay-desktop-cursor.h"

struct _XrdOverlayClient
{
  XrdClient parent;

  gboolean pinned_only;
  XrdOverlayWindow *pinned_button;

  OpenVROverlayUploader *uploader;
};

G_DEFINE_TYPE (XrdOverlayClient, xrd_overlay_client, XRD_TYPE_CLIENT)

static void
xrd_overlay_client_finalize (GObject *gobject);

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               float             ppm,
                               gboolean          is_child,
                               gboolean          follow_head);

gboolean
xrd_overlay_client_add_button (XrdOverlayClient   *self,
                               XrdWindow         **button,
                               int                 label_count,
                               gchar             **label,
                               graphene_point3d_t *position,
                               GCallback           press_callback,
                               gpointer            press_callback_data);

GulkanClient *
xrd_overlay_client_get_uploader (XrdOverlayClient *self);

static void
xrd_overlay_client_class_init (XrdOverlayClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_client_finalize;

  XrdClientClass *xrd_client_class = XRD_CLIENT_CLASS (klass);
  xrd_client_class->add_button =
      (void*) xrd_overlay_client_add_button;
  xrd_client_class->get_uploader =
      (void*) xrd_overlay_client_get_uploader;
}

XrdOverlayClient *
xrd_overlay_client_new (void)
{
  return (XrdOverlayClient*) g_object_new (XRD_TYPE_OVERLAY_CLIENT, 0);
}

static void
xrd_overlay_client_finalize (GObject *gobject)
{
  XrdOverlayClient *self = XRD_OVERLAY_CLIENT (gobject);

  G_OBJECT_CLASS (xrd_overlay_client_parent_class)->finalize (gobject);

  /* Uploader needs to be freed after context! */
  g_object_unref (self->uploader);
}

GulkanClient *
xrd_overlay_client_get_uploader (XrdOverlayClient *self)
{
  return GULKAN_CLIENT (self->uploader);
}

gboolean
xrd_overlay_client_add_button (XrdOverlayClient   *self,
                               XrdWindow         **button,
                               int                 label_count,
                               gchar             **label,
                               graphene_point3d_t *position,
                               GCallback           press_callback,
                               gpointer            press_callback_data)
{
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, position);

  int width = 220;
  int height = 220;
  int ppm = 450;

  unsigned char image[4 * width * height];
  cairo_surface_t* surface =
      xrd_client_create_button_surface (image, width, height, label_count, label);

  GulkanClient *client = GULKAN_CLIENT (self->uploader);
  GulkanTexture *texture =
    gulkan_texture_new_from_cairo_surface (client->device, surface,
                                           VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_cairo_surface (client, texture, surface);

  GString *full_label = g_string_new ("");
  for (int i = 0; i < label_count; i++)
    {
      g_string_append (full_label, label[i]);
      if (i < label_count - 1)
        g_string_append (full_label, " ");
    }

  XrdOverlayWindow *window =
    xrd_overlay_window_new_from_ppm (full_label->str, width, height, ppm);

  g_string_free (full_label, FALSE);

  if (window == NULL)
    return FALSE;

  xrd_window_submit_texture (XRD_WINDOW (window), client, texture);

  *button = XRD_WINDOW (window);

  xrd_window_set_transformation (XRD_WINDOW (window), &transform);

  XrdWindowManager *manager = xrd_client_get_manager (XRD_CLIENT (self));
  xrd_window_manager_add_window (manager,
                                 XRD_WINDOW (*button),
                                 XRD_WINDOW_HOVERABLE |
                                 XRD_WINDOW_DESTROY_WITH_PARENT |
                                 XRD_WINDOW_MANAGER_BUTTON);

  g_signal_connect (window, "grab-start-event",
                    (GCallback) press_callback, press_callback_data);

  xrd_client_add_button_callbacks (XRD_CLIENT (self),
                                   XRD_WINDOW (window));

  return TRUE;
}

static void
xrd_overlay_client_init (XrdOverlayClient *self)
{

  self->pinned_only = FALSE;
  OpenVRContext *openvr_context =
    xrd_client_get_openvr_context (XRD_CLIENT (self));

  if (!openvr_context_init_overlay (openvr_context))
    {
      g_printerr ("Error: Could not init OpenVR application.\n");
      return;
    }
  if (!openvr_context_is_valid (openvr_context))
    {
      g_printerr ("Error: OpenVR context is invalid.\n");
      return;
    }

  self->uploader = openvr_overlay_uploader_new ();
  if (!openvr_overlay_uploader_init_vulkan (self->uploader, false))
    g_printerr ("Unable to initialize Vulkan!\n");

  xrd_client_post_openvr_init (XRD_CLIENT (self));

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      XrdPointer *pointer = XRD_POINTER (xrd_overlay_pointer_new (i));
      if (pointer == NULL)
        {
          g_printerr ("Error: Could not init pointer %d\n", i);
          return;
        }
      xrd_client_set_pointer (XRD_CLIENT (self), pointer, i);


      XrdPointerTip *pointer_tip =
        XRD_POINTER_TIP (xrd_overlay_pointer_tip_new (i, self->uploader));
      if (pointer == NULL)
        {
          g_printerr ("Error: Could not init pointer tip %d\n", i);
          return;
        }
      xrd_client_set_pointer_tip (XRD_CLIENT (self), pointer_tip, i);

      xrd_pointer_tip_init_vulkan (pointer_tip);
      xrd_pointer_tip_set_active (pointer_tip, FALSE);
      xrd_pointer_tip_show (pointer_tip);
    }

  XrdDesktopCursor *cursor =
    XRD_DESKTOP_CURSOR (xrd_overlay_desktop_cursor_new (self->uploader));

  xrd_client_set_desktop_cursor (XRD_CLIENT (self), cursor);
}
