/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib-unix.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <graphene.h>
#include <signal.h>

#include <openvr-glib.h>

#include "openvr-context.h"
#include "openvr-io.h"
#include "openvr-compositor.h"
#include "openvr-math.h"
#include "openvr-overlay.h"
#include "openvr-overlay-uploader.h"
#include "openvr-action.h"
#include "openvr-action-set.h"
#include "xrd-overlay-pointer.h"
#include "xrd-overlay-pointer-tip.h"
#include "xrd-overlay-manager.h"

typedef struct Example
{
  OpenVROverlayUploader *uploader;
  GulkanTexture *texture;

  XrdOverlayManager *manager;

  OpenVRActionSet *wm_action_set;

  XrdOverlayPointer *pointer_overlay;
  XrdOverlayPointerTip *intersection_overlay;
  OpenVROverlay *paint_overlay;

  GdkPixbuf *draw_pixbuf;

  GMainLoop *loop;
} Example;

typedef struct ActionCallbackData
{
  Example *self;
  int      controller_index;
} ActionCallbackData;

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

gboolean
_poll_events_cb (gpointer _self)
{
  Example *self = (Example*) _self;

  if (!openvr_action_set_poll (self->wm_action_set))
    return FALSE;

  return TRUE;
}

GdkPixbuf *
_create_draw_pixbuf (uint32_t width, uint32_t height)
{
  guchar * pixels = (guchar*) malloc (sizeof (guchar) * height * width * 4);
  memset (pixels, 255, height * width * 4 * sizeof (guchar));

  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB,
                                                TRUE, 8, width, height,
                                                4 * width, NULL, NULL);
  return pixbuf;
}

GdkPixbuf *
load_gdk_pixbuf (const gchar* name)
{
  GError * error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource (name, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, false, 0, 0, 0);
  g_object_unref (pixbuf_rgb);
  return pixbuf;
}

typedef struct ColorRGBA
{
  guchar r;
  guchar g;
  guchar b;
  guchar a;
} ColorRGBA;

void
_place_pixel (guchar    *pixels,
              int        n_channels,
              int        rowstride,
              int        x,
              int        y,
              ColorRGBA *color)
{
  guchar *p = pixels + y * rowstride
                     + x * n_channels;

  p[0] = color->r;
  p[1] = color->g;
  p[2] = color->b;
  p[3] = color->a;
}

gboolean
_draw_at_2d_position (Example          *self,
                      PixelSize        *size_pixels,
                      graphene_point_t *position_2d,
                      ColorRGBA        *color,
                      uint32_t          brush_radius)
{
  static GMutex paint_mutex;

  int n_channels = gdk_pixbuf_get_n_channels (self->draw_pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride (self->draw_pixbuf);
  guchar *pixels = gdk_pixbuf_get_pixels (self->draw_pixbuf);

  g_mutex_lock (&paint_mutex);

  for (float x = position_2d->x - (float) brush_radius;
       x <= position_2d->x + (float) brush_radius;
       x++)
    {
      for (float y = position_2d->y - (float) brush_radius;
           y <= position_2d->y + (float) brush_radius;
           y++)
        {
          /* check bounds */
          if (x >= 0 && x <= (float) size_pixels->width &&
              y >= 0 && y <= (float) size_pixels->height)
            {
              graphene_vec2_t put_position;
              graphene_vec2_init (&put_position, x, y);

              graphene_vec2_t brush_center;
              graphene_point_to_vec2 (position_2d, &brush_center);

              graphene_vec2_t distance;
              graphene_vec2_subtract (&put_position, &brush_center, &distance);

              if (graphene_vec2_length (&distance) < brush_radius)
                _place_pixel (pixels, n_channels, rowstride,
                              (int) x, (int) y, color);
            }
        }
    }

  if (!gulkan_client_upload_pixbuf (GULKAN_CLIENT (self->uploader),
                                    self->texture,
                                    self->draw_pixbuf))
    return FALSE;

  openvr_overlay_uploader_submit_frame (self->uploader,
                                       self->paint_overlay, self->texture);

  g_mutex_unlock (&paint_mutex);

  return TRUE;
}

static void
_paint_hover_cb (OpenVROverlay    *overlay,
                 OpenVRHoverEvent *event,
                 gpointer         _self)
{
  Example *self = (Example*) _self;

  xrd_overlay_pointer_tip_update (self->intersection_overlay,
                             &event->pose,
                             &event->point);

  PixelSize size_pixels = {
    .width = (guint) gdk_pixbuf_get_width (self->draw_pixbuf),
    .height = (guint) gdk_pixbuf_get_height (self->draw_pixbuf)
  };

  graphene_point_t position_2d;
  if (!openvr_overlay_get_2d_intersection (overlay,
                                          &event->point,
                                          &size_pixels,
                                          &position_2d))
    return;

  /* check bounds */
  if (position_2d.x < 0 || position_2d.x > size_pixels.width ||
      position_2d.y < 0 || position_2d.y > size_pixels.height)
    return;

  ColorRGBA color = {
    .r = 0,
    .g = 0,
    .b = 0,
    .a = 255
  };

  _draw_at_2d_position (self, &size_pixels, &position_2d, &color, 5);

  xrd_overlay_pointer_set_length (self->pointer_overlay, event->distance);

  free (event);
}

gboolean
_init_paint_overlay (Example *self)
{
  self->draw_pixbuf = _create_draw_pixbuf (1080, 1920);
  if (self->draw_pixbuf == NULL)
    return FALSE;

  self->paint_overlay = openvr_overlay_new ();
  openvr_overlay_create (self->paint_overlay, "pain", "Paint overlay");

  if (!openvr_overlay_is_valid (self->paint_overlay))
    {
      fprintf (stderr, "Overlay unavailable.\n");
      return -1;
    }

  if (!openvr_overlay_set_width_meters (self->paint_overlay, 3.37f))
    return FALSE;

  graphene_point3d_t position = {
    .x = -1,
    .y = 1,
    .z = -3
  };

  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, &position);
  openvr_overlay_set_transform_absolute (self->paint_overlay, &transform);

  if (!openvr_overlay_show (self->paint_overlay))
    return -1;

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  self->texture =
    gulkan_texture_new_from_pixbuf (client->device, self->draw_pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_client_upload_pixbuf (client, self->texture, self->draw_pixbuf);

  openvr_overlay_uploader_submit_frame (self->uploader,
                                       self->paint_overlay, self->texture);

  /* connect glib callbacks */
  //g_signal_connect (self->paint_overlay, "intersection-event",
  //                  (GCallback)_intersection_cb,
  //                  self);
  //
  xrd_overlay_manager_add_overlay (self->manager, self->paint_overlay,
                                      OPENVR_OVERLAY_HOVER);

  openvr_overlay_set_mouse_scale (
    self->paint_overlay,
    (float) gdk_pixbuf_get_width (self->draw_pixbuf),
    (float) gdk_pixbuf_get_height (self->draw_pixbuf));

  g_signal_connect (self->paint_overlay, "hover-event",
                    (GCallback) _paint_hover_cb, self);
  //
  return TRUE;
}

void
_cleanup (Example *self)
{
  g_print ("bye\n");

  g_object_unref (self->intersection_overlay);
  g_object_unref (self->pointer_overlay);
  g_object_unref (self->intersection_overlay);
  g_object_unref (self->texture);

  g_object_unref (self->wm_action_set);

  /* destroy context before uploader because context finalization calls
   * VR_ShutdownInternal() which accesses the vulkan device that is being
   * destroyed by uploader finalization
   */

  OpenVRContext *context = openvr_context_get_instance ();
  g_object_unref (context);

  g_object_unref (self->uploader);
}

static void
_right_hand_pose_cb (OpenVRAction    *action,
                     OpenVRPoseEvent *event,
                     gpointer        _self)
{
  (void) action;
  ActionCallbackData *data = _self;
  Example *self = data->self;

  xrd_overlay_pointer_move (self->pointer_overlay, &event->pose);
  xrd_overlay_manager_update_pose (self->manager, &event->pose, 1);

  /* update intersection */
  //openvr_overlay_poll_3d_intersection (self->paint_overlay, &event->pose);

  g_free (event);
}

void
_no_hover_cb (XrdOverlayManager       *manager,
              OpenVRControllerIndexEvent *event,
              gpointer                   _self)
{
  (void) manager;

  Example *self = (Example*) _self;
  openvr_overlay_hide (OPENVR_OVERLAY (self->intersection_overlay));
  xrd_overlay_pointer_reset_length (self->pointer_overlay);
  g_free (event);
}

int
main ()
{
  OpenVRContext *context = openvr_context_get_instance ();
  if (!openvr_context_init_overlay (context))
    {
      g_printerr ("Could not init OpenVR.\n");
      return false;
    }

  if (!openvr_io_load_cached_action_manifest (
      "openvr-glib",
      "/res/bindings",
      "actions.json",
      "bindings_vive_controller.json",
      "bindings_knuckles_controller.json",
      NULL))
    return -1;

  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .wm_action_set = openvr_action_set_new_from_url ("/actions/wm"),
    .manager = xrd_overlay_manager_new (),
    .uploader = openvr_overlay_uploader_new (),
  };

  if (!openvr_overlay_uploader_init_vulkan (self.uploader, true))
    {
      g_printerr ("Unable to initialize Vulkan!\n");
      return false;
    }

  self.pointer_overlay = xrd_overlay_pointer_new (1);
  if (self.pointer_overlay == NULL)
    return -1;

  self.intersection_overlay = xrd_overlay_pointer_tip_new (1);
  if (self.intersection_overlay == NULL)
    return -1;

  if (!_init_paint_overlay (&self))
    return -1;

  /* TODO: support two controllers */
  ActionCallbackData data_right =
    {
      .self = &self,
      .controller_index = 1
    };
  openvr_action_set_connect (self.wm_action_set, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_right",
                             (GCallback) _right_hand_pose_cb, &data_right);

  g_signal_connect (self.manager, "no-hover-event",
                    (GCallback) _no_hover_cb, &self);

  g_timeout_add (20, _poll_events_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);
  g_main_loop_unref (self.loop);

  _cleanup (&self);

  return 0;
}
