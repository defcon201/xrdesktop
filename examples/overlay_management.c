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
#include <glib/gprintf.h>

#include <openvr-glib.h>

#include "openvr-context.h"
#include "openvr-compositor.h"
#include "openvr-math.h"
#include "openvr-overlay.h"
#include "openvr-overlay-uploader.h"
#include "openvr-io.h"
#include "openvr-action.h"
#include "openvr-action-set.h"
#include "xrd-overlay-pointer.h"
#include "xrd-overlay-pointer-tip.h"
#include "xrd-overlay-button.h"
#include "xrd-overlay-manager.h"

#define GRID_WIDTH 6
#define GRID_HEIGHT 5

#define UPDATE_RATE_MS 20

typedef struct Example
{
  GulkanTexture *texture;

  XrdOverlayManager *manager;

  XrdOverlayPointer *pointer_ray[OPENVR_CONTROLLER_COUNT];
  XrdOverlayPointerTip *pointer_tip[OPENVR_CONTROLLER_COUNT];

  GSList *overlays;

  XrdOverlayButton *button_reset;
  XrdOverlayButton *button_sphere;

  GMainLoop *loop;

  float pointer_default_length;

  OpenVRActionSet *action_set_wm;
  OpenVRActionSet *action_set_mouse_synth;

  OpenVROverlayUploader *uploader;
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

  if (!openvr_action_set_poll (self->action_set_wm))
    return FALSE;

  if (!openvr_action_set_poll (self->action_set_mouse_synth))
    return FALSE;
  return TRUE;
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

void
_overlay_unmark (OpenVROverlay *overlay)
{
  graphene_vec3_t unmarked_color;
  graphene_vec3_init (&unmarked_color, 1.f, 1.f, 1.f);
  openvr_overlay_set_color (overlay, &unmarked_color);
}

void
_overlay_mark_blue (OpenVROverlay *overlay)
{
  graphene_vec3_t marked_color;
  graphene_vec3_init (&marked_color, .2f, .2f, .8f);
  openvr_overlay_set_color (overlay, &marked_color);
}

void
_overlay_mark_green (OpenVROverlay *overlay)
{
  graphene_vec3_t marked_color;
  graphene_vec3_init (&marked_color, .2f, .8f, .2f);
  openvr_overlay_set_color (overlay, &marked_color);
}

void
_overlay_mark_orange (OpenVROverlay *overlay)
{
  graphene_vec3_t marked_color;
  graphene_vec3_init (&marked_color, .8f, .4f, .2f);
  openvr_overlay_set_color (overlay, &marked_color);
}

void
_cat_grab_start_cb (OpenVROverlay *overlay,
                    OpenVRControllerIndexEvent *event,
                    gpointer      _self)
{
  Example *self = (Example*) _self;

  /* don't grab if this overlay is already grabbed */
  if (xrd_overlay_manager_is_grabbed (self->manager, overlay))
    {
      g_free (event);
      return;
    }

  xrd_overlay_manager_drag_start (self->manager, event->index);

  _overlay_mark_green (overlay);
  g_free (event);
}

void
_cat_grab_cb (OpenVROverlay   *overlay,
              OpenVRGrabEvent *event,
              gpointer        _self)
{
  (void) overlay;
  Example *self = (Example*) _self;

  XrdOverlayPointerTip *pointer_tip =
    self->pointer_tip[event->controller_index];
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (pointer_tip),
                                         &event->pose);

  xrd_overlay_pointer_tip_set_constant_width (pointer_tip);
  g_free (event);
}

void
_cat_release_cb (OpenVROverlay *overlay,
                 OpenVRControllerIndexEvent *event,
                 gpointer      _self)
{
  (void) event;
  (void) _self;
  _overlay_unmark (overlay);
  g_free (event);
}

/* This will not be emitted during grabbing */
void
_hover_cb (OpenVROverlay    *overlay,
           OpenVRHoverEvent *event,
           gpointer         _self)
{
  Example *self = (Example*) _self;

  if (!xrd_overlay_manager_is_grabbed (self->manager, overlay))
    _overlay_mark_blue (overlay);

  /* update pointer length and pointer tip overlay */
  XrdOverlayPointerTip *pointer_tip =
    self->pointer_tip[event->controller_index];

  graphene_matrix_t overlay_pose;
  openvr_overlay_get_transform_absolute (overlay, &overlay_pose);

  xrd_overlay_pointer_tip_update (pointer_tip, &overlay_pose, &event->point);

  XrdOverlayPointer *pointer_ray = self->pointer_ray[event->controller_index];
  xrd_overlay_pointer_set_length (pointer_ray, event->distance);
  g_free (event);
}

void
_hover_button_cb (OpenVROverlay    *overlay,
                  OpenVRHoverEvent *event,
                  gpointer         _self)
{
  Example *self = (Example*) _self;

  _overlay_mark_orange (overlay);

  XrdOverlayPointer *pointer_ray =
      self->pointer_ray[event->controller_index];
  XrdOverlayPointerTip *pointer_tip =
      self->pointer_tip[event->controller_index];

  /* update pointer length and intersection overlay */
  graphene_matrix_t overlay_pose;
  openvr_overlay_get_transform_absolute (overlay, &overlay_pose);

  xrd_overlay_pointer_tip_update (pointer_tip, &overlay_pose, &event->point);
  xrd_overlay_pointer_set_length (pointer_ray, event->distance);
  g_free (event);
}

void
_hover_end_cb (OpenVROverlay *overlay,
               OpenVRControllerIndexEvent *event,
               gpointer       _self)
{
  (void) event;
  Example *self = (Example*) _self;

  XrdOverlayPointer *pointer_ray = self->pointer_ray[event->index];
  xrd_overlay_pointer_reset_length (pointer_ray);

  /* unmark if no controller is hovering over this overlay */
  if (!xrd_overlay_manager_is_hovered (self->manager, overlay))
    _overlay_unmark (overlay);

  /* When leaving this overlay and immediately entering another, the tip should
   * still be active because it is now hovering another overlay. */
  gboolean active = self->manager->hover_state[event->index].overlay != NULL;

  XrdOverlayPointerTip *pointer_tip = self->pointer_tip[event->index];
  xrd_overlay_pointer_tip_set_active (pointer_tip, self->uploader, active);
  g_free (event);
}

void
_hover_start_cb (OpenVROverlay *overlay,
                 OpenVRControllerIndexEvent *event,
                 gpointer       _self)
{
  (void) overlay;
  (void) event;
  Example *self = (Example*) _self;

  XrdOverlayPointerTip *pointer_tip = self->pointer_tip[event->index];
  xrd_overlay_pointer_tip_set_active (pointer_tip, self->uploader, TRUE);

  g_free (event);
}

void
_no_hover_cb (XrdOverlayManager *manager,
              OpenVRNoHoverEvent   *event,
              gpointer             _self)
{
  (void) manager;

  Example *self = (Example*) _self;

  XrdOverlayPointerTip *pointer_tip = self->pointer_tip[event->controller_index];

  XrdOverlayPointer *pointer_ray = self->pointer_ray[event->controller_index];

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f, 0.f, -pointer_ray->default_length);

  graphene_matrix_t tip_pose;

  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation, &event->pose);

  graphene_vec3_t controller_translation;
  openvr_math_matrix_get_translation (&event->pose, &controller_translation);
  graphene_point3d_t controller_translation_point;
  graphene_point3d_init_from_vec3 (&controller_translation_point,
                                   &controller_translation);

  graphene_matrix_init_identity (&tip_pose);
  graphene_matrix_translate (&tip_pose, &distance_translation_point);
  graphene_matrix_rotate_quaternion (&tip_pose, &controller_rotation);
  graphene_matrix_translate (&tip_pose, &controller_translation_point);

  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (pointer_tip),
                                         &tip_pose);

  xrd_overlay_pointer_tip_set_constant_width (pointer_tip);

  xrd_overlay_pointer_tip_set_active (pointer_tip, self->uploader, FALSE);

  g_free (event);
}

gboolean
_init_cat_overlays (Example *self)
{
  GdkPixbuf *pixbuf = load_gdk_pixbuf ("/res/hawk.jpg");
  if (pixbuf == NULL)
    return -1;

  GulkanClient *client = GULKAN_CLIENT (self->uploader);

  self->texture = gulkan_texture_new_from_pixbuf (client->device, pixbuf,
                                                  VK_FORMAT_R8G8B8A8_UNORM);

  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);

  float width = .5f;

  float pixbuf_aspect = (float) gdk_pixbuf_get_width (pixbuf) /
                        (float) gdk_pixbuf_get_height (pixbuf);

  float height = width / pixbuf_aspect;

  g_print ("pixbuf: %.2f x %.2f\n", (float) gdk_pixbuf_get_width (pixbuf),
                                    (float) gdk_pixbuf_get_height (pixbuf));
  g_print ("meters: %.2f x %.2f\n", width, height);


  uint32_t i = 0;

  for (float x = 0; x < GRID_WIDTH * width; x += width)
    for (float y = 0; y < GRID_HEIGHT * height; y += height)
      {
        OpenVROverlay *cat = openvr_overlay_new ();

        char overlay_name[10];
        g_sprintf (overlay_name, "cat%d", i);

        openvr_overlay_create_width (cat, overlay_name, "A Cat", width);

        if (!openvr_overlay_is_valid (cat))
          {
            g_printerr ("Overlay unavailable.\n");
            return -1;
          }

        openvr_overlay_set_mouse_scale (cat,
                                        (float) gdk_pixbuf_get_width (pixbuf),
                                        (float) gdk_pixbuf_get_height (pixbuf));

        graphene_point3d_t position = {
          .x = x,
          .y = y,
          .z = -3
        };
        openvr_overlay_set_translation (cat, &position);
        xrd_overlay_manager_add_overlay (self->manager, cat,
                                            OPENVR_OVERLAY_HOVER |
                                            OPENVR_OVERLAY_GRAB |
                                            OPENVR_OVERLAY_DESTROY_WITH_PARENT);

        openvr_overlay_uploader_submit_frame (self->uploader, cat,
                                             self->texture);

        g_signal_connect (cat, "grab-start-event",
                          (GCallback)_cat_grab_start_cb, self);
        g_signal_connect (cat, "grab-event",
                          (GCallback)_cat_grab_cb, self);
        g_signal_connect (cat, "release-event",
                          (GCallback)_cat_release_cb, self);
        g_signal_connect (cat, "hover-start-event",
                          (GCallback) _hover_start_cb, self);
        g_signal_connect (cat, "hover-event",
                          (GCallback) _hover_cb, self);
        g_signal_connect (cat, "hover-end-event",
                          (GCallback) _hover_end_cb, self);

        self->overlays = g_slist_prepend (self->overlays, cat);

        if (!openvr_overlay_show (cat))
          return -1;

        i++;
      }

  g_object_unref (pixbuf);

  return TRUE;
}

gboolean
_init_button (Example            *self,
              XrdOverlayButton      **button,
              gchar              *id,
              gchar              *label,
              graphene_point3d_t *position,
              GCallback           callback)
{
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, position);

  *button = xrd_overlay_button_new (id, label);
  if (*button == NULL)
    return FALSE;

  OpenVROverlay *overlay = OPENVR_OVERLAY (*button);

  openvr_overlay_set_transform_absolute (overlay, &transform);

  xrd_overlay_manager_add_overlay (self->manager, overlay,
                                      OPENVR_OVERLAY_HOVER);

  if (!openvr_overlay_set_width_meters (overlay, 0.5f))
    return FALSE;

  g_signal_connect (overlay, "grab-start-event", (GCallback) callback, self);
  g_signal_connect (overlay, "hover-event", (GCallback) _hover_button_cb, self);
  g_signal_connect (overlay, "hover-end-event",
                    (GCallback) _hover_end_cb, self);

  return TRUE;
}

void
_button_sphere_press_cb (OpenVROverlay   *overlay,
                         OpenVRControllerIndexEvent *event,
                         gpointer        _self)
{
  (void) overlay;
  (void) event;
  Example *self = (Example*) _self;
  xrd_overlay_manager_arrange_sphere (self->manager,
                                         GRID_WIDTH,
                                         GRID_HEIGHT);
  g_free (event);
}

void
_button_reset_press_cb (OpenVROverlay   *overlay,
                        OpenVRControllerIndexEvent *event,
                        gpointer        _self)
{
  (void) overlay;
  (void) event;
  Example *self = (Example*) _self;
  xrd_overlay_manager_arrange_reset (self->manager);
  g_free (event);
}

gboolean
_init_buttons (Example *self)
{
  graphene_point3d_t position_reset = {
    .x =  0.0f,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!_init_button (self, &self->button_reset,
                     "control.reset", "Reset", &position_reset,
                     (GCallback) _button_reset_press_cb))
    return FALSE;

  graphene_point3d_t position_sphere = {
    .x =  0.5f,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!_init_button (self, &self->button_sphere,
                     "control.sphere", "Sphere", &position_sphere,
                     (GCallback) _button_sphere_press_cb))
    return FALSE;

  return TRUE;
}

static void
_hand_pose_cb (OpenVRAction    *action,
               OpenVRPoseEvent *event,
               gpointer         _self)
{
  (void) action;
  ActionCallbackData *data = _self;
  Example *self = (Example*) data->self;
  XrdOverlayPointer *pointer_ray = self->pointer_ray[data->controller_index];

  xrd_overlay_manager_update_pose (self->manager, &event->pose,
                                      data->controller_index);
  xrd_overlay_pointer_move (pointer_ray, &event->pose);
  g_free (event);
}

static void
_grab_cb (OpenVRAction       *action,
          OpenVRDigitalEvent *event,
          gpointer            _self)
{
  (void) action;

  ActionCallbackData *data = _self;
  Example *self = (Example*) data->self;

  if (event->changed)
    {
      if (event->state == 1)
        xrd_overlay_manager_check_grab (self->manager,
                                           data->controller_index);
      else
        xrd_overlay_manager_check_release (self->manager,
                                              data->controller_index);
    }

  g_free (event);
}

#define SCROLL_TO_PUSH_RATIO 2
#define SCALE_FACTOR 0.75
#define ANALOG_TRESHOLD 0.000001

static void
_action_push_pull_scale_cb (OpenVRAction      *action,
                            OpenVRAnalogEvent *event,
                            gpointer          _self)
{
  (void) action;
  ActionCallbackData *data = _self;
  Example *self = data->self;

  GrabState *grab_state =
      &self->manager->grab_state[data->controller_index];

  float x_state = graphene_vec3_get_x (&event->state);
  if (grab_state->overlay && fabs (x_state) > ANALOG_TRESHOLD)
    {
      float factor = x_state * SCALE_FACTOR;
      xrd_overlay_manager_scale (self->manager, grab_state, factor,
                                    UPDATE_RATE_MS);
    }

  float y_state = graphene_vec3_get_y (&event->state);
  if (grab_state->overlay && fabs (y_state) > ANALOG_TRESHOLD)
    {
      HoverState *hover_state =
        &self->manager->hover_state[data->controller_index];
      hover_state->distance +=
        SCROLL_TO_PUSH_RATIO *
        hover_state->distance *
        graphene_vec3_get_y (&event->state) *
        (UPDATE_RATE_MS / 1000.);

      XrdOverlayPointer *pointer_ray = self->pointer_ray[data->controller_index];
      xrd_overlay_pointer_set_length (pointer_ray, hover_state->distance);
    }

  g_free (event);
}

static void
_destroy_overlay (gpointer _overlay,
                  gpointer _unused)
{
  (void) _unused;
  OpenVROverlay *overlay = _overlay;
  g_object_unref (overlay);
}

void
_cleanup (Example *self)
{
  g_main_loop_unref (self->loop);

  g_print ("bye\n");

  g_slist_foreach (self->overlays, (GFunc) _destroy_overlay, NULL);
  g_slist_free (self->overlays);

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      g_object_unref (self->pointer_ray[i]);
      g_object_unref (self->pointer_tip[i]);
    }
  g_object_unref (self->texture);

  g_object_unref (self->button_reset);
  g_object_unref (self->button_sphere);

  g_object_unref (self->action_set_wm);
  g_object_unref (self->action_set_mouse_synth);

  g_object_unref (self->manager);

  OpenVRContext *context = openvr_context_get_instance ();
  g_object_unref (context);

  g_object_unref (self->uploader);
}

static void
_action_left_click_cb (OpenVRAction       *action,
                       OpenVRDigitalEvent *event,
                       ActionCallbackData *data)
{
  (void) action;
  if (event->changed && event->state)
    {
      HoverState *hover_state =
          &data->self->manager->hover_state[data->controller_index];
      if (hover_state->overlay != NULL)
        {
          XrdOverlayPointerTip *pointer_tip =
              data->self->pointer_tip[data->controller_index];
          xrd_overlay_pointer_tip_animate_pulse (pointer_tip, data->self->uploader);
        }
    }
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
    .action_set_wm = openvr_action_set_new_from_url ("/actions/wm"),
    .action_set_mouse_synth = openvr_action_set_new_from_url ("/actions/mouse_synth"),
    .manager = xrd_overlay_manager_new (),
    .pointer_default_length = 5.0
  };

  self.uploader = openvr_overlay_uploader_new ();
  if (!openvr_overlay_uploader_init_vulkan (self.uploader, true))
    {
      g_printerr ("Unable to initialize Vulkan!\n");
      return false;
    }

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      self.pointer_ray[i] = xrd_overlay_pointer_new (i);
      if (self.pointer_ray[i] == NULL)
        return -1;
      // openvr_overlay_hide (OPENVR_OVERLAY (self.pointer_ray[i]));
      self.pointer_tip[i] = xrd_overlay_pointer_tip_new (i);
      if (self.pointer_tip[i] == NULL)
        return -1;
      xrd_overlay_pointer_tip_init_vulkan (self.pointer_tip[i], self.uploader);
      xrd_overlay_pointer_tip_set_active (self.pointer_tip[i], self.uploader,
                                      FALSE);
      openvr_overlay_show (OPENVR_OVERLAY (self.pointer_tip[i]));
    }

  if (!_init_cat_overlays (&self))
    return -1;

  if (!_init_buttons (&self))
    return -1;

  ActionCallbackData data_left =
    {
      .self = &self,
      .controller_index = 0
    };
  ActionCallbackData data_right =
    {
      .self = &self,
      .controller_index = 1
    };

  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_left",
                             (GCallback) _hand_pose_cb, &data_left);
  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_right",
                             (GCallback) _hand_pose_cb, &data_right);
  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_left",
                             (GCallback) _grab_cb, &data_left);
  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_right",
                             (GCallback) _grab_cb, &data_right);

  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_left",
                             (GCallback) _action_push_pull_scale_cb,
                             &data_left);
  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_right",
                             (GCallback) _action_push_pull_scale_cb,
                             &data_right);

  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_left",
                             (GCallback) _action_push_pull_scale_cb,
                             &data_left);
  openvr_action_set_connect (self.action_set_wm, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_right",
                             (GCallback) _action_push_pull_scale_cb,
                             &data_right);

  openvr_action_set_connect (self.action_set_mouse_synth, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_left",
                             (GCallback) _action_left_click_cb, &data_left);
  openvr_action_set_connect (self.action_set_mouse_synth, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_right",
                             (GCallback) _action_left_click_cb, &data_right);

  g_signal_connect (self.manager, "no-hover-event",
                    (GCallback) _no_hover_cb, &self);

  g_timeout_add (UPDATE_RATE_MS, _poll_events_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);

  return 0;
}
