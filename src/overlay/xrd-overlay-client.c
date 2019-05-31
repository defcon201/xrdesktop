/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-overlay-client.h"

#include <openvr-glib.h>

#include <gdk/gdk.h>
#include <glib/gprintf.h>
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

  GulkanClient *gc;
};

G_DEFINE_TYPE (XrdOverlayClient, xrd_overlay_client, XRD_TYPE_CLIENT)

static void
xrd_overlay_client_finalize (GObject *gobject);

static void
xrd_overlay_client_init (XrdOverlayClient *self)
{
  xrd_client_set_upload_layout (XRD_CLIENT (self),
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  self->pinned_only = FALSE;
}

static bool
_init_openvr (XrdOverlayClient *self)
{
 OpenVRContext *context =
    xrd_client_get_openvr_context (XRD_CLIENT (self));

  if (!openvr_context_initialize (context, OPENVR_APP_OVERLAY))
    {
      g_printerr ("Error: Could not init OpenVR application.\n");
      return false;
    }
  return true;
}

XrdOverlayClient *
xrd_overlay_client_new (void)
{
  XrdOverlayClient *self =
    (XrdOverlayClient*) g_object_new (XRD_TYPE_OVERLAY_CLIENT, 0);

  if (!_init_openvr (self))
    {
      g_object_unref (self);
      return NULL;
    }

  self->gc = openvr_compositor_gulkan_client_new (true);
  if (!self->gc)
    {
      g_printerr ("Unable to initialize Vulkan!\n");
      g_object_unref (self);
      return NULL;
    }

  xrd_client_post_openvr_init (XRD_CLIENT (self));

  XrdDesktopCursor *cursor =
    XRD_DESKTOP_CURSOR (xrd_overlay_desktop_cursor_new ());
  xrd_client_set_desktop_cursor (XRD_CLIENT (self), cursor);

  return self;
}

static void
xrd_overlay_client_finalize (GObject *gobject)
{
  XrdOverlayClient *self = XRD_OVERLAY_CLIENT (gobject);

  G_OBJECT_CLASS (xrd_overlay_client_parent_class)->finalize (gobject);

  /* Uploader needs to be freed after context! */
  g_object_unref (self->gc);
}

static GulkanClient *
_get_uploader (XrdClient *client)
{
  XrdOverlayClient *self = XRD_OVERLAY_CLIENT (client);
  return self->gc;
}

static gboolean
_add_button (XrdClient          *client,
             XrdWindow         **button,
             int                 label_count,
             gchar             **label,
             graphene_point3d_t *position,
             GCallback           press_callback,
             gpointer            press_callback_data)
{
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, position);

  uint32_t width = 220;
  uint32_t height = 220;
  float ppm = 450;

  GString *full_label = g_string_new ("");
  for (int i = 0; i < label_count; i++)
    {
      g_string_append (full_label, label[i]);
      if (i < label_count - 1)
        g_string_append (full_label, " ");
    }

  XrdWindow *window =
    XRD_WINDOW (xrd_overlay_window_new_from_ppm (full_label->str,
                                                 width, height, ppm));

  g_string_free (full_label, FALSE);

  if (window == NULL)
    return FALSE;

  GulkanClient *gc = xrd_client_get_uploader (client);

  VkImageLayout layout = xrd_client_get_upload_layout (client);

  xrd_button_set_text (window, gc, layout, label_count, label);

  *button = window;

  xrd_window_set_transformation (window, &transform);

  XrdWindowManager *manager = xrd_client_get_manager (client);
  xrd_window_manager_add_window (manager,
                                 *button,
                                 XRD_WINDOW_HOVERABLE |
                                 XRD_WINDOW_DESTROY_WITH_PARENT |
                                 XRD_WINDOW_MANAGER_BUTTON);

  g_signal_connect (window, "grab-start-event",
                    (GCallback) press_callback, press_callback_data);

  xrd_client_add_button_callbacks (client, window);

  return TRUE;
}

static void
_init_controller (XrdClient     *client,
                  XrdController *controller)
{
  GulkanClient *gc = xrd_client_get_uploader (client);
  guint64 controller_handle = xrd_controller_get_handle (controller);
  XrdPointer *pointer_ray =
    XRD_POINTER (xrd_overlay_pointer_new (controller_handle));
  if (pointer_ray == NULL)
    {
      g_printerr ("Error: Could not init pointer %lu\n", controller_handle);
      return;
    }
  xrd_controller_set_pointer (controller, pointer_ray);

  XrdPointerTip *pointer_tip =
    XRD_POINTER_TIP (xrd_overlay_pointer_tip_new (controller_handle, gc));
  if (pointer_tip == NULL)
    {
      g_printerr ("Error: Could not init pointer tip %lu\n", controller_handle);
      return;
    }

  xrd_pointer_tip_set_active (pointer_tip, FALSE);
  xrd_pointer_tip_show (pointer_tip);

  xrd_controller_set_pointer_tip (controller, pointer_tip);
}

static void
xrd_overlay_client_class_init (XrdOverlayClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_client_finalize;

  XrdClientClass *xrd_client_class = XRD_CLIENT_CLASS (klass);
  xrd_client_class->add_button = _add_button;
  xrd_client_class->get_uploader = _get_uploader;
  xrd_client_class->init_controller = _init_controller;
}
