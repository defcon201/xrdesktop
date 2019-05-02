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
#include "xrd-settings.h"
#include <openvr-math.h>
#include "xrd-math.h"
#include "xrd-client.h"
#include "graphene-ext.h"

struct _XrdOverlayClient
{
  XrdClient parent;

  OpenVRContext *context;

  XrdOverlayPointer *pointer_ray[OPENVR_CONTROLLER_COUNT];
  XrdOverlayPointerTip *pointer_tip[OPENVR_CONTROLLER_COUNT];

  XrdWindowManager *manager;

  XrdClientController left;
  XrdClientController right;

  XrdWindow *button_reset;
  XrdWindow *button_sphere;

  OpenVROverlayUploader *uploader;

  OpenVRActionSet *wm_actions;

  XrdWindow *hover_window[OPENVR_CONTROLLER_COUNT];
  XrdWindow *keyboard_window;
  guint keyboard_press_signal;
  guint keyboard_close_signal;

  int poll_rate_ms;
  guint poll_event_source_id;

  double analog_threshold;

  double scroll_to_push_ratio;
  double scroll_to_scale_ratio;

  double pixel_per_meter;

  XrdInputSynth *input_synth;

  XrdOverlayDesktopCursor *cursor;
};

G_DEFINE_TYPE (XrdOverlayClient, xrd_overlay_client, XRD_TYPE_CLIENT)

static void
xrd_overlay_client_finalize (GObject *gobject);

static void
xrd_overlay_client_class_init (XrdOverlayClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_client_finalize;

  XrdClientClass *xrd_client_class = XRD_CLIENT_CLASS (klass);
  xrd_client_class->add_window =
      (void*) xrd_overlay_client_add_window;
  xrd_client_class->add_button =
      (void*) xrd_overlay_client_add_button;
  xrd_client_class->remove_window =
      (void*) xrd_overlay_client_remove_window;
  xrd_client_class->save_reset_transform =
      (void*) xrd_overlay_client_save_reset_transform;
  xrd_client_class->get_keyboard_window =
      (void*) xrd_overlay_client_get_keyboard_window;
  xrd_client_class->get_uploader =
      (void*) xrd_overlay_client_get_uploader;
  xrd_client_class->get_synth_hovered =
      (void*) xrd_overlay_client_get_synth_hovered;
  xrd_client_class->submit_cursor_texture =
      (void*) xrd_overlay_client_submit_cursor_texture;
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

  g_source_remove (self->poll_event_source_id);

  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      g_object_unref (self->pointer_ray[i]);
      g_object_unref (self->pointer_tip[i]);
    }

  g_object_unref (self->cursor);

  g_object_unref (self->wm_actions);

  g_object_unref (self->manager);

  /* TODO: disabling scene client does not mean we shut down VR */
  g_object_unref (self->context);
  self->context = NULL;

  /* Uploader needs to be freed after context! */
  g_object_unref (self->uploader);

  xrd_settings_destroy_instance ();

  G_OBJECT_CLASS (xrd_overlay_client_parent_class)->finalize (gobject);
}

GulkanClient *
xrd_overlay_client_get_uploader (XrdOverlayClient *self)
{
  return GULKAN_CLIENT (self->uploader);
}

XrdWindowManager *
xrd_overlay_client_get_manager (XrdOverlayClient *self)
{
  return self->manager;
}

XrdOverlayDesktopCursor *
xrd_overlay_client_get_cursor (XrdOverlayClient *self)
{
  return self->cursor;
}

XrdWindow *
xrd_overlay_client_get_keyboard_window (XrdOverlayClient *self)
{
  return self->keyboard_window;
}

void
xrd_overlay_client_save_reset_transform (XrdOverlayClient *self,
                                         XrdWindow *window)
{
  xrd_window_manager_save_reset_transform (self->manager, window);
}

XrdWindow *
xrd_overlay_client_get_synth_hovered (XrdOverlayClient *self)
{
  int controller = xrd_input_synth_synthing_controller (self->input_synth);
  XrdWindow *parent =
      xrd_window_manager_get_hover_state (self->manager, controller)->window;
  return parent;
}

static void
_action_hand_pose_cb (OpenVRAction            *action,
                      OpenVRPoseEvent         *event,
                      XrdClientController     *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;
  xrd_window_manager_update_pose (self->manager, &event->pose,
                                          controller->index);

  XrdOverlayPointer *pointer = self->pointer_ray[controller->index];
  xrd_overlay_pointer_move (pointer, &event->pose);

  /* show cursor while synth controller hovers window, but doesn't grab */
  if (controller->index ==
          xrd_input_synth_synthing_controller (self->input_synth) &&
      self->hover_window[controller->index] != NULL &&
      xrd_window_manager_get_grab_state
          (self->manager, controller->index)->window == NULL)
    xrd_overlay_desktop_cursor_show (self->cursor);

  g_free (event);
}

static void
_action_push_pull_scale_cb (OpenVRAction        *action,
                            OpenVRAnalogEvent   *event,
                            XrdClientController *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;

  GrabState *grab_state =
      xrd_window_manager_get_grab_state (self->manager, controller->index);

  float x_state = graphene_vec3_get_x (&event->state);
  if (grab_state->window && fabs (x_state) > self->analog_threshold)
    {
      float factor = x_state * self->scroll_to_scale_ratio;
      xrd_window_manager_scale (self->manager, grab_state, factor,
                                        self->poll_rate_ms);
    }

  float y_state = graphene_vec3_get_y (&event->state);
  if (grab_state->window && fabs (y_state) > self->analog_threshold)
    {
      HoverState *hover_state =
        xrd_window_manager_get_hover_state (self->manager, controller->index);
      hover_state->distance +=
        self->scroll_to_push_ratio *
        hover_state->distance *
        graphene_vec3_get_y (&event->state) *
        (self->poll_rate_ms / 1000.);

      XrdOverlayPointer *pointer_ray = self->pointer_ray[controller->index];
      xrd_overlay_pointer_set_length (pointer_ray, hover_state->distance);
    }

  g_free (event);
}

static void
_action_grab_cb (OpenVRAction        *action,
                 OpenVRDigitalEvent  *event,
                 XrdClientController *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;
  if (event->changed)
    {
      if (event->state == 1)
        xrd_window_manager_check_grab (self->manager, controller->index);
      else
        xrd_window_manager_check_release (self->manager, controller->index);
    }

  g_free (event);
}

static void
_action_rotate_cb (OpenVRAction        *action,
                   OpenVRAnalogEvent  *event,
                   XrdClientController *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;
  GrabState *grab_state =
      xrd_window_manager_get_grab_state (self->manager, controller->index);

  float force = graphene_vec3_get_x (&event->state);

  /* Start rotating when pressed with some force, not just touched. */
  float threshold = 0.5;
  float slerp_factor = .05 * (((force - threshold) / (1 - threshold)));

  if (force > threshold && grab_state->window != NULL)
    {
      graphene_quaternion_t id;
      graphene_quaternion_init_identity (&id);
      graphene_quaternion_slerp (&grab_state->window_transformed_rotation_neg,
                                 &id, slerp_factor,
                                 &grab_state->window_transformed_rotation_neg);
      graphene_quaternion_slerp (&grab_state->window_rotation,
                                 &id, slerp_factor,
                                 &grab_state->window_rotation);
    }
  g_free (event);
}

void
_window_grab_start_cb (XrdOverlayWindow        *window,
                       XrdControllerIndexEvent *event,
                       gpointer                 _self)
{
  XrdOverlayClient *self = _self;

  /* don't grab if this overlay is already grabbed */
  if (xrd_window_manager_is_grabbed (self->manager, XRD_WINDOW (window)))
    {
      g_free (event);
      return;
    }

  xrd_window_manager_drag_start (self->manager, event->index);

  if (event->index == xrd_input_synth_synthing_controller (self->input_synth))
    xrd_overlay_desktop_cursor_hide (self->cursor);

  g_free (event);
}

void
_window_grab_cb (XrdOverlayWindow *window,
                 XrdGrabEvent     *event,
                 gpointer          _self)
{
  (void) window;
  XrdOverlayClient *self = (XrdOverlayClient*) _self;

  XrdOverlayPointerTip *pointer_tip =
    self->pointer_tip[event->controller_index];
  xrd_overlay_pointer_tip_set_transformation_matrix (pointer_tip, &event->pose);

  xrd_overlay_pointer_tip_set_constant_width (pointer_tip);
  g_free (event);
}

/* TODO: class hierarchie*/
void
xrd_window_unmark (XrdWindow *self)
{
  graphene_vec3_t unmarked_color;
  graphene_vec3_init (&unmarked_color, 1.f, 1.f, 1.f);
  xrd_window_set_color (self, &unmarked_color);
}

void
xrd_window_mark_color (XrdWindow *self, float r, float g, float b)
{
  graphene_vec3_t marked_color;
  //graphene_vec3_init (&marked_color, .8f, .4f, .2f);
  graphene_vec3_init (&marked_color, r, g, b);
  xrd_window_set_color (self, &marked_color);
}

void
_button_hover_cb (XrdWindow     *window,
                  XrdHoverEvent *event,
                  gpointer       _self)
{
  XrdOverlayClient *self = _self;

  xrd_window_mark_color (window, .8f, .4f, .2f);

  XrdOverlayPointer *pointer =
      self->pointer_ray[event->controller_index];
  XrdOverlayPointerTip *pointer_tip =
      self->pointer_tip[event->controller_index];

  /* update pointer length and intersection overlay */
  graphene_matrix_t window_pose;
  xrd_window_get_transformation_matrix (window, &window_pose);

  xrd_overlay_pointer_tip_update (pointer_tip, &window_pose, &event->point);
  xrd_overlay_pointer_set_length (pointer, event->distance);

  g_free (event);
}

void
_window_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdOverlayClient *self = (XrdOverlayClient*) _self;

  XrdOverlayPointer *pointer_ray = self->pointer_ray[event->index];
  xrd_overlay_pointer_reset_length (pointer_ray);

  /* When leaving this window but now hovering another, the tip should
   * still be active because it is now hovering another window. */
  gboolean active =
      xrd_window_manager_get_hover_state (self->manager, event->index)->window
          != NULL;

  XrdOverlayPointerTip *pointer_tip = self->pointer_tip[event->index];
  xrd_overlay_pointer_tip_set_active (pointer_tip, active);

  xrd_input_synth_reset_press_state (self->input_synth);

  if (event->index == xrd_input_synth_synthing_controller (self->input_synth))
    xrd_overlay_desktop_cursor_hide (self->cursor);

  g_free (event);
}

void
_button_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  XrdOverlayClient *self = (XrdOverlayClient*) _self;

  /* unmark if no controller is hovering over this overlay */
  if (!xrd_window_manager_is_hovered (self->manager, window))
    xrd_window_unmark (window);

  _window_hover_end_cb (window, event, _self);
}

/* 3DUI buttons */

static cairo_surface_t*
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
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  cairo_fill (cr);

  double r0;
  if (width < height)
    r0 = (double) width / 3.0;
  else
    r0 = (double) height / 3.0;

  double radius = r0 * 4.0;
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

  float font_size = 52.0;

  cairo_set_font_size (cr, font_size);

  cairo_text_extents_t extents;
  cairo_text_extents (cr, text, &extents);

  cairo_move_to (cr,
                 center_x - extents.width / 2,
                 center_y  - extents.height / 2 + extents.height / 2);
  cairo_set_source_rgb (cr, 0.9, 0.9, 0.9);
  cairo_show_text (cr, text);

  cairo_destroy (cr);

  return surface;
}

gboolean
xrd_overlay_client_add_button (XrdOverlayClient   *self,
                               XrdWindow         **button,
                               gchar              *label,
                               graphene_point3d_t *position,
                               GCallback           press_callback,
                               gpointer            press_callback_data)
{
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, position);


  int width = 220;
  int height = 120;

  int ppm = 450;

  unsigned char image[4 * width * height];
  cairo_surface_t* surface =
      _create_cairo_surface (image, width, height, label);

  GulkanClient *client = GULKAN_CLIENT (self->uploader);
  GulkanTexture *texture = gulkan_texture_new_from_cairo_surface (client->device, surface, VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_cairo_surface (client, texture, surface);

  XrdOverlayWindow *overlay_window = xrd_overlay_window_new (label, ppm, NULL);
  if (overlay_window == NULL)
    return FALSE;

  xrd_overlay_window_submit_texture (overlay_window, client, texture);

  *button = XRD_WINDOW (overlay_window);

  xrd_overlay_window_set_transformation_matrix (overlay_window, &transform);

  xrd_window_manager_add_window (self->manager,
                                 XRD_WINDOW (*button),
                                 XRD_WINDOW_HOVERABLE |
                                 XRD_WINDOW_DESTROY_WITH_PARENT);

  g_signal_connect (overlay_window, "grab-start-event",
                    (GCallback) press_callback, press_callback_data);

  g_signal_connect (overlay_window, "hover-event",
                    (GCallback) _button_hover_cb, self);

  g_signal_connect (overlay_window, "hover-end-event",
                    (GCallback)_button_hover_end_cb, self);

  return TRUE;
}

void
_button_sphere_press_cb (XrdOverlayWindow        *window,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdOverlayClient *self = _self;
  xrd_window_manager_arrange_sphere (self->manager);
  g_free (event);
}

void
_button_reset_press_cb (XrdOverlayWindow        *window,
                        XrdControllerIndexEvent *event,
                        gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdOverlayClient *self = _self;
  xrd_window_manager_arrange_reset (self->manager);
  g_free (event);
}

gboolean
_init_buttons (XrdOverlayClient *self)
{
  float button_x = 0.0f;
  graphene_point3d_t position_reset = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_overlay_client_add_button (self, &self->button_reset, "Reset",
                                      &position_reset,
                                      (GCallback) _button_reset_press_cb,
                                      self))
    return FALSE;

  float reset_width_meter;
  xrd_window_get_width_meter (XRD_WINDOW (self->button_reset),
                              &reset_width_meter);

  button_x += reset_width_meter;

  graphene_point3d_t position_sphere = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_overlay_client_add_button (self, &self->button_sphere, "Sphere",
                                      &position_sphere,
                                      (GCallback) _button_sphere_press_cb,
                                      self))
    return FALSE;

  return TRUE;
}

static void
_keyboard_press_cb (OpenVRContext    *context,
                    GdkEventKey      *event,
                    XrdOverlayClient *self)
{
  (void) context;
  xrd_client_emit_keyboard_press (XRD_CLIENT (self), event);
}

static void
_keyboard_close_cb (OpenVRContext    *context,
                    XrdOverlayClient *self)
{
  self->keyboard_window = NULL;

  g_signal_handler_disconnect(context, self->keyboard_press_signal);
  g_signal_handler_disconnect(context, self->keyboard_close_signal);
  self->keyboard_press_signal = 0;
  self->keyboard_close_signal = 0;

  g_print ("Keyboard closed\n");
}

static void
_action_show_keyboard_cb (OpenVRAction       *action,
                          OpenVRDigitalEvent *event,
                          XrdOverlayClient   *self)
{
  (void) action;
  if (!event->state && event->changed)
    {
      OpenVRContext *context = openvr_context_get_instance ();
      openvr_context_show_system_keyboard (context);

      /* TODO: Perhaps there is a better way to get the window that should
               receive keyboard input */
      int controller = xrd_input_synth_synthing_controller (self->input_synth);
      self->keyboard_window = xrd_window_manager_get_hover_state
          (self->manager, controller)->window;

      self->keyboard_press_signal =
          g_signal_connect (context, "keyboard-press-event",
                            (GCallback) _keyboard_press_cb, self);
      self->keyboard_close_signal =
          g_signal_connect (context, "keyboard-close-event",
                            (GCallback) _keyboard_close_cb, self);
    }
}

void
_window_hover_cb (XrdWindow *window,
                  XrdHoverEvent    *event,
                  XrdOverlayClient *self)
{
  /* update pointer length and intersection overlay */
  XrdOverlayPointerTip *pointer_tip =
    self->pointer_tip[event->controller_index];

  graphene_matrix_t window_pose;
  xrd_window_get_transformation_matrix (window, &window_pose);
  xrd_overlay_pointer_tip_update (pointer_tip, &window_pose, &event->point);

  XrdOverlayPointer *pointer = self->pointer_ray[event->controller_index];
  xrd_overlay_pointer_set_length (pointer, event->distance);

  self->hover_window[event->controller_index] = window;

  if (event->controller_index ==
      xrd_input_synth_synthing_controller (self->input_synth))
    {
      xrd_input_synth_move_cursor (self->input_synth, window, &event->point);

      XrdOverlayWindow *owindow = XRD_OVERLAY_WINDOW (window);
      xrd_overlay_desktop_cursor_update (self->cursor, owindow, &event->point);

      if (self->hover_window[event->controller_index] != window)
        xrd_input_synth_reset_scroll (self->input_synth);
    }
}

void
_window_hover_start_cb (XrdOverlayWindow        *window,
                        XrdControllerIndexEvent *event,
                        XrdOverlayClient        *self)
{
  (void) window;
  (void) event;

  XrdOverlayPointerTip *pointer_tip = self->pointer_tip[event->index];
  xrd_overlay_pointer_tip_set_active (pointer_tip, TRUE);

  g_free (event);
}

void
_manager_no_hover_cb (XrdWindowManager *manager,
                      XrdNoHoverEvent  *event,
                      XrdOverlayClient *self)
{
  (void) manager;

  XrdOverlayPointerTip *pointer_tip =
    self->pointer_tip[event->controller_index];

  XrdOverlayPointer *pointer_ray = self->pointer_ray[event->controller_index];

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f,
                         0.f,
                         -xrd_overlay_pointer_get_default_length (pointer_ray));

  graphene_matrix_t tip_pose;

  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation, &event->pose);

  graphene_point3d_t controller_translation_point;
  graphene_matrix_get_translation_point3d (&event->pose,
                                           &controller_translation_point);

  graphene_matrix_init_identity (&tip_pose);
  graphene_matrix_translate (&tip_pose, &distance_translation_point);
  graphene_matrix_rotate_quaternion (&tip_pose, &controller_rotation);
  graphene_matrix_translate (&tip_pose, &controller_translation_point);

  xrd_overlay_pointer_tip_set_transformation_matrix (pointer_tip, &tip_pose);

  xrd_overlay_pointer_tip_set_constant_width (pointer_tip);

  xrd_overlay_pointer_tip_set_active (pointer_tip, FALSE);

  g_free (event);

  xrd_input_synth_reset_scroll (self->input_synth);

  self->hover_window[event->controller_index] = NULL;
}

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               float             ppm,
                               gboolean          is_child,
                               gboolean          follow_head)
{
  gchar *window_title = g_strdup (title);
  if (!window_title)
    window_title = g_strdup ("Unnamed Window");

  XrdOverlayWindow *window = xrd_overlay_window_new (window_title, ppm, native);

  XrdWindowFlags flags = XRD_WINDOW_HOVERABLE | XRD_WINDOW_DESTROY_WITH_PARENT;

  /* User can't drag child windows, they are attached to the parent.
   * The child window's position is managed by its parent, not the WM. */
  if (!is_child && !follow_head)
    flags |= XRD_WINDOW_DRAGGABLE | XRD_WINDOW_MANAGED;

  if (follow_head)
      flags |= XRD_WINDOW_FOLLOW_HEAD;

  xrd_window_manager_add_window (self->manager, XRD_WINDOW (window), flags);
  g_signal_connect (window, "grab-start-event",
                    (GCallback) _window_grab_start_cb, self);
  g_signal_connect (window, "grab-event",
                    (GCallback) _window_grab_cb, self);
  // g_signal_connect (window, "release-event",
  //                   (GCallback) _overlay_release_cb, self);
  g_signal_connect (window, "hover-start-event",
                    (GCallback) _window_hover_start_cb, self);
  g_signal_connect (window, "hover-event",
                    (GCallback) _window_hover_cb, self);
  g_signal_connect (window, "hover-end-event",
                    (GCallback) _window_hover_end_cb, self);

  return window;
}

void
xrd_overlay_client_remove_window (XrdOverlayClient *self,
                                  XrdOverlayWindow *window)
{
  xrd_window_manager_remove_window (self->manager, XRD_WINDOW (window));
}

gboolean
xrd_overlay_client_poll_events_cb (gpointer _self)
{
  XrdOverlayClient *self = _self;
  if (!self->context)
    return FALSE;

  openvr_context_poll_event (self->context);

  if (!openvr_action_set_poll (self->wm_actions))
    return FALSE;

  if (xrd_window_manager_is_hovering (self->manager) &&
      !xrd_window_manager_is_grabbing (self->manager))
    if (!xrd_input_synth_poll_events (self->input_synth))
      return FALSE;

  xrd_window_manager_poll_window_events (self->manager);

  return TRUE;
}

static void
_update_double_val (GSettings *settings, gchar *key, gpointer user_data)
{
  double *val = user_data;
  *val = g_settings_get_double (settings, key);
}

static void
_update_poll_rate (GSettings *settings, gchar *key, gpointer user_data)
{
  XrdOverlayClient *self = user_data;
  if (self->poll_event_source_id != 0)
    g_source_remove (self->poll_event_source_id);
  self->poll_rate_ms = g_settings_get_int (settings, key);

  self->poll_event_source_id = g_timeout_add (self->poll_rate_ms,
                                              xrd_overlay_client_poll_events_cb,
                                              self);
}

static void
_synth_click_cb (XrdInputSynth    *synth,
                 XrdClickEvent    *event,
                 XrdOverlayClient *self)
{
  (void) synth;
  if (self->hover_window[event->controller_index])
    {
      event->window = self->hover_window[event->controller_index];
      xrd_client_emit_click (XRD_CLIENT (self), event);

      if (event->button == 1)
        {
          HoverState *hover_state =
              xrd_window_manager_get_hover_state
                  (self->manager, event->controller_index);
          if (hover_state->window != NULL && event->state)
            {
              XrdOverlayPointerTip *pointer_tip =
                  self->pointer_tip[event->controller_index];
              xrd_overlay_pointer_tip_animate_pulse (pointer_tip);
            }
        }
    }
}

static void
_synth_move_cursor_cb (XrdInputSynth      *synth,
                       XrdMoveCursorEvent *event,
                       XrdOverlayClient   *self)
{
  (void) synth;
  xrd_client_emit_move_cursor (XRD_CLIENT (self), event);
}

static void _system_quit_cb (OpenVRContext *context,
                             GdkEvent *event,
                             XrdOverlayClient *self)
{
  (void) event;
  /* g_print("Handling VR quit event\n"); */
  openvr_context_acknowledge_quit (context);
  xrd_client_emit_system_quit (XRD_CLIENT (self), event);
}

void
xrd_overlay_client_submit_cursor_texture (XrdOverlayClient *self,
                                          GulkanClient *client,
                                          GulkanTexture *texture,
                                          int hotspot_x,
                                          int hotspot_y)
{
  xrd_overlay_desktop_cursor_submit_texture (self->cursor, client, texture,
                                             hotspot_x, hotspot_y);
}

static void
xrd_overlay_client_init (XrdOverlayClient *self)
{
  xrd_settings_connect_and_apply (G_CALLBACK (_update_double_val),
                                  "scroll-to-push-ratio",
                                  &self->scroll_to_push_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (_update_double_val),
                                  "scroll-to-scale-ratio",
                                  &self->scroll_to_scale_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (_update_double_val),
                                  "analog-threshold", &self->analog_threshold);

  self->poll_event_source_id = 0;

  self->context = openvr_context_get_instance ();
  if (!openvr_context_init_overlay (self->context))
    {
      g_printerr ("Error: Could not init OpenVR application.\n");
      return;
    }
  if (!openvr_context_is_valid (self->context))
    {
      g_printerr ("Error: OpenVR context is invalid.\n");
      return;
    }

  g_signal_connect(self->context, "quit-event",
                   (GCallback)_system_quit_cb, self);

  self->uploader = openvr_overlay_uploader_new ();
  if (!openvr_overlay_uploader_init_vulkan (self->uploader, false))
    g_printerr ("Unable to initialize Vulkan!\n");

  self->manager = xrd_window_manager_new ();

  self->keyboard_window = NULL;
  self->keyboard_press_signal = 0;
  self->keyboard_close_signal = 0;

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      self->pointer_ray[i] = xrd_overlay_pointer_new (i);
      if (self->pointer_ray[i] == NULL)
        return;

      self->pointer_tip[i] = xrd_overlay_pointer_tip_new (i, self->uploader);
      if (self->pointer_tip[i] == NULL)
        return;

      self->hover_window[i] = NULL;

      xrd_overlay_pointer_tip_init_vulkan (self->pointer_tip[i]);
      xrd_overlay_pointer_tip_set_active (self->pointer_tip[i], FALSE);
      xrd_overlay_pointer_tip_show (self->pointer_tip[i]);
    }

  _init_buttons (self);

  self->left.self = self;
  self->left.index = 0;

  self->right.self = self;
  self->right.index = 1;

  if (!openvr_io_load_cached_action_manifest (
        "xrdesktop",
        "/res/bindings",
        "actions.json",
        "bindings_vive_controller.json",
        "bindings_knuckles_controller.json",
        NULL))
    {
      g_print ("Failed to load action bindings!\n");
      return;
    }

  self->cursor = xrd_overlay_desktop_cursor_new (self->uploader);

  self->wm_actions = openvr_action_set_new_from_url ("/actions/wm");

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_left",
                             (GCallback) _action_hand_pose_cb, &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_right",
                             (GCallback) _action_hand_pose_cb, &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_left",
                             (GCallback) _action_grab_cb, &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_right",
                             (GCallback) _action_grab_cb, &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/rotate_window_left",
                             (GCallback) _action_rotate_cb, &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/rotate_window_right",
                             (GCallback) _action_rotate_cb, &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_left",
                             (GCallback) _action_push_pull_scale_cb,
                            &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_right",
                             (GCallback) _action_push_pull_scale_cb,
                            &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_left",
                             (GCallback) _action_push_pull_scale_cb,
                            &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_right",
                             (GCallback) _action_push_pull_scale_cb,
                            &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_left",
                             (GCallback) _action_show_keyboard_cb, self);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_right",
                             (GCallback) _action_show_keyboard_cb, self);

  g_signal_connect (self->manager, "no-hover-event",
                    (GCallback) _manager_no_hover_cb, self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_poll_rate),
                                  "event-update-rate-ms", self);

  self->input_synth = xrd_input_synth_new ();

  g_signal_connect (self->input_synth, "click-event",
                    (GCallback) _synth_click_cb, self);
  g_signal_connect (self->input_synth, "move-cursor-event",
                    (GCallback) _synth_move_cursor_cb, self);
}
