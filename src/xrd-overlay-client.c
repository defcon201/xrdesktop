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

/* TODO: Move this to configuration */
#define scroll_threshold 0.1
#define SCROLL_TO_PUSH_RATIO 2

G_DEFINE_TYPE (XrdOverlayClient, xrd_overlay_client, G_TYPE_OBJECT)

enum {
  KEYBOARD_PRESS_EVENT,
  CLICK_EVENT,
  MOVE_CURSOR_EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
xrd_overlay_client_finalize (GObject *gobject);

static void
xrd_overlay_client_class_init (XrdOverlayClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_client_finalize;

  signals[KEYBOARD_PRESS_EVENT] =
    g_signal_new ("keyboard-press-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[CLICK_EVENT] =
    g_signal_new ("click-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[MOVE_CURSOR_EVENT] =
    g_signal_new ("move-cursor-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
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
      g_object_unref (self->pointer[i]);
      g_object_unref (self->intersection[i]);
    }

  g_object_unref (self->synth_actions);
  g_object_unref (self->wm_actions);
  g_object_unref (self->manager);


  g_object_unref (self->context);
  self->context = NULL;

  /* Uploader needs to be freed after context! */
  g_object_unref (self->vk_uploader);

  g_hash_table_unref (self->overlays_to_windows);
}

static void
_action_hand_pose_cb (OpenVRAction            *action,
                      OpenVRPoseEvent         *event,
                      XrdClientController     *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;
  xrd_overlay_manager_update_pose (self->manager, &event->pose,
                                   controller->index);

  XrdOverlayPointer *pointer = self->pointer[controller->index];
  xrd_overlay_pointer_move (pointer, &event->pose);
  g_free (event);
}

static void
_action_push_pull_cb (OpenVRAction        *action,
                      OpenVRAnalogEvent   *event,
                      XrdClientController *controller)
{
  (void) action;
  XrdOverlayClient *self = controller->self;

  GrabState *grab_state =
      &self->manager->grab_state[controller->index];

  if (grab_state->overlay != NULL)
    {
      HoverState *hover_state =
        &self->manager->hover_state[controller->index];
      XrdOverlayPointer *pointer_overlay =
        self->pointer[controller->index];

      hover_state->distance +=
        SCROLL_TO_PUSH_RATIO * graphene_vec3_get_y (&event->state);
      xrd_overlay_pointer_set_length (pointer_overlay,
                                 hover_state->distance);
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
        xrd_overlay_manager_check_grab (self->manager, controller->index);
      else
        xrd_overlay_manager_check_release (self->manager, controller->index);
    }

  g_free (event);
}

void
_overlay_grab_start_cb (OpenVROverlay              *overlay,
                        OpenVRControllerIndexEvent *event,
                        gpointer                    _self)
{
  (void) overlay;
  XrdOverlayClient *self = (XrdOverlayClient*) _self;
  xrd_overlay_manager_drag_start (self->manager, event->index);
  openvr_overlay_hide (OPENVR_OVERLAY (self->intersection[event->index]));
}

void
_overlay_grab_cb (OpenVROverlay   *overlay,
                  OpenVRGrabEvent *event,
                  gpointer        _self)
{
  (void) overlay;
  XrdOverlayClient *self = (XrdOverlayClient*) _self;

  XrdOverlayPointerTip *intersection =
    self->intersection[event->controller_index];
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (intersection),
                                         &event->pose);
  g_free (event);
}

void
_overlay_unmark (OpenVROverlay *overlay)
{
  graphene_vec3_t unmarked_color;
  graphene_vec3_init (&unmarked_color, 1.f, 1.f, 1.f);
  openvr_overlay_set_color (overlay, &unmarked_color);
}

void
_overlay_mark_orange (OpenVROverlay *overlay)
{
  graphene_vec3_t marked_color;
  graphene_vec3_init (&marked_color, .8f, .4f, .2f);
  openvr_overlay_set_color (overlay, &marked_color);
}

void
_hover_button_cb (OpenVROverlay    *overlay,
                  OpenVRHoverEvent *event,
                  gpointer         _self)
{
  XrdOverlayClient *self = _self;

  _overlay_mark_orange (overlay);

  XrdOverlayPointer *pointer =
      self->pointer[event->controller_index];
  XrdOverlayPointerTip *intersection =
      self->intersection[event->controller_index];

  /* update pointer length and intersection overlay */
  xrd_overlay_pointer_tip_update (intersection, &event->pose, &event->point);
  xrd_overlay_pointer_set_length (pointer, event->distance);
}

void
_hover_end_cb (OpenVROverlay *overlay,
               gpointer       unused)
{
  (void) unused;
  _overlay_unmark (overlay);
}

/* 3DUI buttons */
gboolean
_init_button (XrdOverlayClient   *self,
              XrdOverlayButton  **button,
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

  xrd_overlay_manager_add_overlay (self->manager, overlay, OPENVR_OVERLAY_HOVER);

  if (!openvr_overlay_set_width_meters (overlay, 0.5f))
    return FALSE;

  g_signal_connect (overlay, "grab-start-event", (GCallback) callback, self);
  g_signal_connect (overlay, "hover-event", (GCallback) _hover_button_cb, self);
  g_signal_connect (overlay, "hover-end-event",
                    (GCallback) _hover_end_cb, NULL);

  return TRUE;
}

void
_button_sphere_press_cb (OpenVROverlay             *overlay,
                         OpenVRControllerIndexEvent *event,
                         gpointer                  _self)
{
  (void) event;
  (void) overlay;
  XrdOverlayClient *self = _self;
  xrd_overlay_manager_arrange_sphere (self->manager, 5, 5);
}

void
_button_reset_press_cb (OpenVROverlay              *overlay,
                        OpenVRControllerIndexEvent *event,
                        gpointer                   _self)
{
  (void) event;
  (void) overlay;
  XrdOverlayClient *self = _self;
  xrd_overlay_manager_arrange_reset (self->manager);
}

gboolean
_init_buttons (XrdOverlayClient *self)
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
_keyboard_press_cb (OpenVROverlay    *overlay,
                    GdkEventKey      *event,
                    XrdOverlayClient *self)
{
  (void) overlay;
  g_signal_emit (self, signals[KEYBOARD_PRESS_EVENT], 0, event);
}

static void
_keyboard_close_cb (OpenVROverlay    *overlay,
                    XrdOverlayClient *self)
{
  (void) overlay;
  (void) self;
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

      g_signal_connect (context, "keyboard-press-event",
                        (GCallback) _keyboard_press_cb, self);
      g_signal_connect (context, "keyboard-close-event",
                        (GCallback) _keyboard_close_cb, self);
    }
}

void
_emit_click (XrdOverlayClient *self,
             XrdOverlayWindow *window,
             graphene_point_t *position,
             int               button,
             gboolean          state)
{
  XrdClickEvent *click_event = g_malloc (sizeof (XrdClickEvent));
  click_event->window = window;
  click_event->position = position;
  click_event->button = button;
  click_event->state = state;
  g_signal_emit (self, signals[CLICK_EVENT], 0, click_event);
}

static void
_action_left_click_cb (OpenVRAction       *action,
                       OpenVRDigitalEvent *event,
                       XrdOverlayClient   *self)
{
  (void) action;
  if (self->hover_window && event->changed)
    {
      _emit_click (self, self->hover_window,
                  &self->hover_position, 1, event->state);

      if (event->state)
        self->button_press_state |= 1 << 1;
      else
        self->button_press_state &= ~(1 << 1);
    }
  g_free (event);
}

static void
_action_right_click_cb (OpenVRAction       *action,
                        OpenVRDigitalEvent *event,
                        XrdOverlayClient   *self)
{
  (void) action;
  if (self->hover_window && event->changed)
    {
      _emit_click (self, self->hover_window,
              &self->hover_position, 3, event->state);
      if (event->state)
        self->button_press_state |= 1 << 3;
      else
        self->button_press_state &= ~(1 << 3);
    }
  g_free (event);
}

static void
_do_scroll (XrdOverlayClient *self, int steps_x, int steps_y)
{
  for (int i = 0; i < abs(steps_y); i++)
    {
      int btn;
      if (steps_y > 0)
        btn = 4;
      else
        btn = 5;
      _emit_click (self, self->hover_window, &self->hover_position, btn, TRUE);
      _emit_click (self, self->hover_window, &self->hover_position, btn, FALSE);
    }

  for (int i = 0; i < abs(steps_x); i++)
    {
      int btn;
      if (steps_x < 0)
        btn = 6;
      else
        btn = 7;
      _emit_click (self, self->hover_window, &self->hover_position, btn, TRUE);
      _emit_click (self, self->hover_window, &self->hover_position, btn, FALSE);
    }
}

/*
 * When the touchpad is touched, start adding up movement.
 * If movement is over threshold, create a scroll event and reset
 * scroll_accumulator.
 */
static void
_action_scroll_cb (OpenVRAction      *action,
                   OpenVRAnalogEvent *event,
                   XrdOverlayClient  *self)
{
  (void) action;
  /* When z is not zero we get bogus data. We ignore this completely */
  if (graphene_vec3_get_z (&event->state) != 0.0)
    return;

  graphene_vec3_add (&self->scroll_accumulator, &event->state,
                     &self->scroll_accumulator);

  float x_acc = graphene_vec3_get_x (&self->scroll_accumulator);
  float y_acc = graphene_vec3_get_y (&self->scroll_accumulator);

  /*
   * Scroll as many times as the threshold has been exceeded.
   * e.g. user scrolled 0.32 with threshold of 0.1 -> scroll 3 times.
   */
  int steps_x = x_acc / scroll_threshold;
  int steps_y = y_acc / scroll_threshold;

  /*
   * We need to keep the rest in the accumulator to not lose part of the
   * user's movement e.g. 0.32: -> 0.2 and -0.32 -> -0.2
   */
  float rest_x = x_acc - (float)steps_x * scroll_threshold;
  float rest_y = y_acc - (float)steps_y * scroll_threshold;
  graphene_vec3_init (&self->scroll_accumulator, rest_x, rest_y, 0);

  _do_scroll (self, steps_x, steps_y);

  g_free (event);
}

void
_emit_move_cursor (XrdOverlayClient *self,
                   XrdOverlayWindow *window,
                   graphene_point_t *position)
{
  XrdMoveCursorEvent *event = g_malloc (sizeof (XrdClickEvent));
  event->window = window;
  event->position = position;
  g_signal_emit (self, signals[MOVE_CURSOR_EVENT], 0, event);
}

void
_overlay_hover_cb (OpenVROverlay    *overlay,
                   OpenVRHoverEvent *event,
                   XrdOverlayClient *self)
{
  XrdOverlayWindow *win = g_hash_table_lookup (self->overlays_to_windows,
                                               overlay);

  /* update pointer length and intersection overlay */
  XrdOverlayPointerTip *intersection =
    self->intersection[event->controller_index];
  xrd_overlay_pointer_tip_update (intersection, &event->pose, &event->point);

  XrdOverlayPointer *pointer = self->pointer[event->controller_index];
  xrd_overlay_pointer_set_length (pointer, event->distance);

  PixelSize size_pixels;
  openvr_overlay_get_size_pixels (overlay, &size_pixels);
  graphene_point_t position_2d;
  if (!openvr_overlay_get_2d_intersection (overlay,
                                          &event->point,
                                          &size_pixels,
                                          &position_2d))
    return;

  _emit_move_cursor (self, win, &position_2d);

  if (self->hover_window != win)
    graphene_vec3_init (&self->scroll_accumulator, 0, 0, 0);

  self->hover_window = win;
  graphene_point_init_from_point (&self->hover_position, &position_2d);
}

void
_overlay_hover_end_cb (OpenVROverlay              *overlay,
                       OpenVRControllerIndexEvent *event,
                       XrdOverlayClient           *self)
{
  (void) event;
  XrdOverlayWindow *win =  g_hash_table_lookup (self->overlays_to_windows,
                                                overlay);

  if (self && self->button_press_state)
    {
      /*g_print ("End hover, button mask %d...\n", self->button_press_state); */
      for (int button = 1; button <= 8; button++)
        {
          gboolean pressed = self->button_press_state & 1 << button;
          if (pressed)
            {
              g_print ("Released button %d\n", button);
              _emit_click (self, win, &self->hover_position, button, FALSE);
            }
        }
      self->button_press_state = 0;
    }
}

void
_manager_no_hover_cb (XrdOverlayManager  *manager,
                      OpenVRNoHoverEvent *event,
                      XrdOverlayClient   *self)
{
  (void) manager;
  XrdOverlayPointer *pointer_overlay = self->pointer[event->controller_index];
  XrdOverlayPointerTip *intersection =
    self->intersection[event->controller_index];

  openvr_overlay_hide (OPENVR_OVERLAY (intersection));
  xrd_overlay_pointer_reset_length (pointer_overlay);

  graphene_vec3_init (&self->scroll_accumulator, 0, 0, 0);
  self->hover_window = NULL;
}

XrdOverlayWindow *
xrd_overlay_client_add_window (XrdOverlayClient *self,
                               const char       *title,
                               gpointer          native,
                               uint32_t          width,
                               uint32_t          height)
{
  gchar *window_title = g_strdup (title);
  if (!window_title)
    window_title = g_strdup ("Unnamed Window");

  gchar overlay_id_str [25];
  g_sprintf (overlay_id_str, "xrd-window-%d", self->new_overlay_index);
  self->new_overlay_index++;
  g_print ("Creating overlay with id %s: \"%s\"\n",
           overlay_id_str,
           window_title);

  OpenVROverlay *overlay = openvr_overlay_new ();

  openvr_overlay_create (overlay, overlay_id_str, window_title);
  g_free (window_title);

  if (!openvr_overlay_is_valid (overlay))
  {
    g_printerr ("Overlay unavailable.\n");
    return FALSE;
  }

   /* Mouse scale is required for the intersection test */
  openvr_overlay_set_mouse_scale (overlay, width, height);

  XrdOverlayWindow *window = xrd_overlay_window_new ();
  window->native = native;
  window->overlay = overlay;

  /* This has to be 0 for first upload, to trigger resolution change. */
  window->width = width;
  window->height = width;

  openvr_overlay_show (overlay);

  g_hash_table_insert (self->overlays_to_windows, overlay, window);

  xrd_overlay_manager_add_overlay (self->manager, overlay,
                                   OPENVR_OVERLAY_HOVER |
                                   OPENVR_OVERLAY_GRAB |
                                   OPENVR_OVERLAY_DESTROY_WITH_PARENT);

  g_signal_connect (overlay, "grab-start-event",
                    (GCallback) _overlay_grab_start_cb, self);
  g_signal_connect (overlay, "grab-event",
                    (GCallback) _overlay_grab_cb, self);
  // g_signal_connect (overlay, "release-event",
  //                   (GCallback) _overlay_release_cb, win);

  g_signal_connect (overlay, "hover-event",
                    (GCallback) _overlay_hover_cb, self);
  g_signal_connect (overlay, "hover-end-event",
                    (GCallback) _overlay_hover_end_cb, self);

  return window;
}

void
xrd_overlay_client_remove_window (XrdOverlayClient *self,
                                  XrdOverlayWindow *window)
{
  if (!g_hash_table_remove (self->overlays_to_windows, window->overlay))
    {
      g_printerr ("Error: Could not remove overlay, key not found.\n");
      return;
    }

  xrd_overlay_manager_remove_overlay (self->manager, window->overlay);
}

gboolean
_poll_events (gpointer _self)
{
  XrdOverlayClient *self = _self;
  if (!self->context)
    return FALSE;

  openvr_context_poll_event (self->context);
  openvr_action_set_poll (self->wm_actions);
  if (xrd_overlay_manager_is_hovering (self->manager) &&
      !xrd_overlay_manager_is_grabbing (self->manager))
    openvr_action_set_poll (self->synth_actions);


  xrd_overlay_manager_poll_overlay_events (self->manager);

  return TRUE;
}

static void
xrd_overlay_client_init (XrdOverlayClient *self)
{
  self->hover_window = NULL;
  self->button_press_state = 0;
  self->new_overlay_index = 0;

  graphene_point_init (&self->hover_position, 0, 0);

  self->overlays_to_windows = g_hash_table_new_full (g_direct_hash,
                                                     g_direct_equal,
                                                     NULL, g_object_unref);

  self->context = openvr_context_get_instance ();
  g_assert (openvr_context_init_overlay (self->context));
  g_assert (openvr_context_is_valid (self->context));

  self->vk_uploader = openvr_overlay_uploader_new ();
  if (!openvr_overlay_uploader_init_vulkan (self->vk_uploader, false))
    g_printerr ("Unable to initialize Vulkan!\n");

  self->manager = xrd_overlay_manager_new ();

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      self->pointer[i] = xrd_overlay_pointer_new (i);
      if (self->pointer[i] == NULL)
        return;
      self->intersection[i] = xrd_overlay_pointer_tip_new (i);
      if (self->intersection[i] == NULL)
        return;

      xrd_overlay_pointer_tip_init_vulkan (self->intersection[i],
                                           self->vk_uploader);
      xrd_overlay_pointer_tip_set_active (self->intersection[i],
                                          self->vk_uploader, FALSE);
      openvr_overlay_show (OPENVR_OVERLAY (self->intersection[i]));
    }

  _init_buttons (self);

  self->left.self = self;
  self->left.index = 0;

  self->right.self = self;
  self->right.index = 1;

  if (!openvr_io_load_cached_action_manifest (
        "gnome-shell",
        "/res/vr-input-bindings",
        "gnome-shell-actions.json",
        "gnome-shell-bindings-vive-controller.json",
        "gnome-shell-bindings-knuckles-controller.json",
        NULL))
    {
      g_print ("Failed to load action bindings!\n");
      return;
    }

  self->wm_actions =
    openvr_action_set_new_from_url ("/actions/wm");

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
                             "/actions/wm/in/push_pull_left",
                             (GCallback) _action_push_pull_cb, &self->left);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_right",
                             (GCallback) _action_push_pull_cb, &self->right);

  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_left",
                             (GCallback) _action_show_keyboard_cb, self);
  openvr_action_set_connect (self->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_right",
                             (GCallback) _action_show_keyboard_cb, self);

  self->synth_actions =
    openvr_action_set_new_from_url ("/actions/mouse_synth");

    /*
   * TODO: Implement active Desktop synth hand
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_left",
                             (GCallback) _action_left_click_cb, self);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/right_click_left",
                             (GCallback) _action_right_click_cb, self);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_ANALOG,
                             "/actions/mouse_synth/in/scroll_left",
                             (GCallback) _action_scroll_cb, self);
  */
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_right",
                             (GCallback) _action_left_click_cb, self);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/right_click_right",
                             (GCallback) _action_right_click_cb, self);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_ANALOG,
                             "/actions/mouse_synth/in/scroll_right",
                             (GCallback) _action_scroll_cb, self);

  g_signal_connect (self->manager, "no-hover-event",
                    (GCallback) _manager_no_hover_cb, self);

  self->poll_event_source_id = g_timeout_add (20, _poll_events, self);
}
