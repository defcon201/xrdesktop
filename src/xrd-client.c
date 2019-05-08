/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-client.h"

#include <openvr-io.h>

#include "graphene-ext.h"

#include "xrd-pointer.h"
#include "xrd-pointer-tip.h"
#include "xrd-desktop-cursor.h"
#include "xrd-settings.h"

enum {
  KEYBOARD_PRESS_EVENT,
  CLICK_EVENT,
  MOVE_CURSOR_EVENT,
  REQUEST_QUIT_EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _XrdClientPrivate
{
  GObject parent;

  OpenVRContext *context;
  XrdWindowManager *manager;
  OpenVRActionSet *wm_actions;
  XrdInputSynth *input_synth;

  XrdWindow *button_reset;
  XrdWindow *button_sphere;

  gboolean pinned_only;
  XrdWindow *pinned_button;

  gboolean selection_mode;
  XrdWindow *select_pinned_button;

  XrdWindow *hover_window[OPENVR_CONTROLLER_COUNT];
  XrdWindow *keyboard_window;

  guint keyboard_press_signal;
  guint keyboard_close_signal;

  guint poll_runtime_event_source_id;
  guint poll_input_source_id;
  int poll_input_rate_ms;

  double analog_threshold;

  double scroll_to_push_ratio;
  double scroll_to_scale_ratio;

  double pixel_per_meter;

  XrdPointer *pointer_ray[OPENVR_CONTROLLER_COUNT];
  XrdPointerTip *pointer_tip[OPENVR_CONTROLLER_COUNT];
  XrdDesktopCursor *cursor;

  XrdClientController controllers[OPENVR_CONTROLLER_COUNT];

} XrdClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XrdClient, xrd_client, G_TYPE_OBJECT)

static void
xrd_client_finalize (GObject *gobject);

gboolean
_init_buttons (XrdClient *self);

static void
xrd_client_class_init (XrdClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_client_finalize;

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

  signals[REQUEST_QUIT_EVENT] =
    g_signal_new ("request-quit-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 0, 0);
}

/**
 * xrd_client_add_window:
 * @self: The #XrdClient
 * @title: An arbitrary title for the window.
 * @native: A user pointer that should be used for associating a native window
 * struct (or wrapper) with the created #XrdWindow.
 * @ppm: The initial pixel per meter setting for this #XrdWindow.
 * @is_child: If true, the window can not be dragged with controllers and will
 * not be otherwise managed by the window manager. For windows that have this
 * attribute set, xrd_window_add_child() should be called on a desired parent
 * window.
 * @follow_head: An #XrdWindow with this attribute will move to keep its
 * current distance from the user and will move to stay in the user's view.
 *
 * Creates an #XrdWindow, puts it under the management of the #XrdWindowManager
 * and returns it.
 */
void
xrd_client_add_window (XrdClient *self,
                       XrdWindow *window,
                       gboolean   is_child,
                       gboolean   follow_head)
{
  XrdWindowFlags flags = XRD_WINDOW_HOVERABLE | XRD_WINDOW_DESTROY_WITH_PARENT;

  /* User can't drag child windows, they are attached to the parent.
   * The child window's position is managed by its parent, not the WM. */
  if (!is_child && !follow_head)
    flags |= XRD_WINDOW_DRAGGABLE | XRD_WINDOW_MANAGED;

  if (follow_head)
      flags |= XRD_WINDOW_FOLLOW_HEAD;

  XrdWindowManager *manager = xrd_client_get_manager (self);
  xrd_window_manager_add_window (manager, window, flags);

  xrd_client_add_window_callbacks (self, window);
}

/**
 * xrd_client_add_button:
 * @self: The #XrdClient
 * @button: The button (#XrdWindow) that will be created by this function.
 * @label: Text that will be displayed on the button.
 * @position: World space position of the button.
 * @press_callback: A function that will be called when the button is grabbed.
 * @press_callback_data: User pointer passed to @press_callback.
 */
gboolean
xrd_client_add_button (XrdClient          *self,
                       XrdWindow         **button,
                       const gchar        *label,
                       graphene_point3d_t *position,
                       GCallback           press_callback,
                       gpointer            press_callback_data)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->add_button == NULL)
      return FALSE;
  return klass->add_button (self, button, label, position,
                            press_callback, press_callback_data);
}

void
xrd_client_set_pin (XrdClient *self,
                    XrdWindow *win,
                    gboolean pin)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_set_pin (priv->manager, win, pin);
}

void
xrd_client_show_pinned_only (XrdClient *self,
                             gboolean pinned_only)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_show_pinned_only (priv->manager, pinned_only);
}


/**
 * xrd_client_get_keyboard_window
 * @self: The #XrdClient
 *
 * Returns: The window that is currently used for keyboard input. Can be NULL.
 */
XrdWindow *
xrd_client_get_keyboard_window (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->keyboard_window;
}

GulkanClient *
xrd_client_get_uploader (XrdClient *self)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->get_uploader == NULL)
      return FALSE;
  return klass->get_uploader (self);
}

/**
 * xrd_client_get_synth_hovered:
 * @self: The #XrdClient
 *
 * Returns: If the controller used for synthesizing input is hovering over an
 * #XrdWindow, return this window, else NULL.
 */
XrdWindow *
xrd_client_get_synth_hovered (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  int controller = xrd_input_synth_synthing_controller (priv->input_synth);
  XrdWindow *parent =
      xrd_window_manager_get_hover_state (priv->manager, controller)->window;
  return parent;
}

/**
 * xrd_client_submit_cursor_texture:
 * @self: The #XrdClient
 * @client: A GulkanClient, for example an OpenVROverlayUploader.
 * @texture: A GulkanTexture that is created and owned by the caller.
 * For performance reasons it is a good idea for the caller to reuse this
 * texture.
 * @hotspot_x: The x component of the hotspot.
 * @hotspot_y: The x component of the hotspot.
 *
 * A hotspot of (x, y) means that the hotspot is at x pixels right, y pixels
 * down from the top left corner of the texture.
 */
void
xrd_client_submit_cursor_texture (XrdClient     *self,
                                  GulkanClient  *client,
                                  GulkanTexture *texture,
                                  int            hotspot_x,
                                  int            hotspot_y)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_desktop_cursor_submit_texture (priv->cursor, client, texture,
                                     hotspot_x, hotspot_y);
}

void
xrd_client_emit_keyboard_press (XrdClient *self,
                                GdkEventKey *event)
{
  g_signal_emit (self, signals[KEYBOARD_PRESS_EVENT], 0, event);
}

void
xrd_client_emit_click (XrdClient *self,
                       XrdClickEvent *event)
{
  g_signal_emit (self, signals[CLICK_EVENT], 0, event);
}

void
xrd_client_emit_move_cursor (XrdClient *self,
                             XrdMoveCursorEvent *event)
{
  g_signal_emit (self, signals[MOVE_CURSOR_EVENT], 0, event);
}

void
xrd_client_emit_system_quit (XrdClient *self,
                             GdkEvent *event)
{
  g_signal_emit (self, signals[REQUEST_QUIT_EVENT], 0, event);
}

XrdClient *
xrd_client_new (void)
{
  return (XrdClient*) g_object_new (XRD_TYPE_CLIENT, 0);
}

static void
xrd_client_finalize (GObject *gobject)
{
  XrdClient *self = XRD_CLIENT (gobject);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  g_source_remove (priv->poll_runtime_event_source_id);
  g_source_remove (priv->poll_input_source_id);

  g_object_unref (priv->manager);
  g_object_unref (priv->wm_actions);

  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      g_object_unref (priv->pointer_ray[i]);
      g_object_unref (priv->pointer_tip[i]);
    }

  g_object_unref (priv->cursor);

  /* TODO: should this be freed? */
  // g_object_unref (priv->input_synth);

  g_clear_object (&priv->context);

  xrd_settings_destroy_instance ();

  G_OBJECT_CLASS (xrd_client_parent_class)->finalize (gobject);
}

OpenVRContext *
xrd_client_get_openvr_context (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->context;
}

XrdWindowManager *
xrd_client_get_manager (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->manager;
}

/**
 * xrd_client_save_reset_transform:
 * @self: The #XrdClient
 * @window: The #XrdWindow to save the current transform for. The reset
 * functionality of #XrdWindowManager will reset the transform of this window
 * to the transform the window has when this function is called.
 */
void
xrd_client_save_reset_transform (XrdClient *self,
                                 XrdWindow *window)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_save_reset_transform (priv->manager, window);
}

/**
 * xrd_client_remove_window:
 * @self: The #XrdClient
 * @window: The #XrdWindow to remove.
 *
 * Removes an #XrdWindow from the management of the #XrdClient and the
 * #XrdWindowManager.
 * Note that the #XrdWindow will not be destroyed by this function.
 */
void
xrd_client_remove_window (XrdClient *self,
                          XrdWindow *window)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_remove_window (priv->manager, window);
}

OpenVRActionSet *
xrd_client_get_wm_actions (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->wm_actions;
}

XrdInputSynth *
xrd_client_get_input_synth (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->input_synth;
}

gboolean
xrd_client_poll_runtime_events (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (!priv->context)
    return FALSE;

  openvr_context_poll_event (priv->context);
  return TRUE;
}

gboolean
xrd_client_poll_input_events (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (!priv->context)
    return FALSE;

  if (!openvr_action_set_poll (priv->wm_actions))
    return FALSE;

  if (xrd_window_manager_is_hovering (priv->manager) &&
      !xrd_window_manager_is_grabbing (priv->manager))
    if (!xrd_input_synth_poll_events (priv->input_synth))
      return FALSE;

  xrd_window_manager_poll_window_events (priv->manager);

  return TRUE;
}

XrdDesktopCursor *
xrd_client_get_cursor (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->cursor;
}

static void
_action_hand_pose_cb (OpenVRAction            *action,
                      OpenVRPoseEvent         *event,
                      XrdClientController     *controller)
{
  (void) action;
  XrdClient *self = controller->self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  xrd_window_manager_update_pose (priv->manager, &event->pose,
                                  controller->index);

  XrdPointer *pointer = priv->pointer_ray[controller->index];
  xrd_pointer_move (pointer, &event->pose);

  /* show cursor while synth controller hovers window, but doesn't grab */
  if (controller->index ==
          xrd_input_synth_synthing_controller (priv->input_synth) &&
      priv->hover_window[controller->index] != NULL &&
      xrd_window_manager_get_grab_state
          (priv->manager, controller->index)->window == NULL)
    xrd_desktop_cursor_show (priv->cursor);

  g_free (event);
}

static void
_action_push_pull_scale_cb (OpenVRAction        *action,
                            OpenVRAnalogEvent   *event,
                            XrdClientController *controller)
{
  (void) action;
  XrdClient *self = controller->self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  GrabState *grab_state =
      xrd_window_manager_get_grab_state (priv->manager, controller->index);

  float x_state = graphene_vec3_get_x (&event->state);
  if (grab_state->window && fabs (x_state) > priv->analog_threshold)
    {
      float factor = x_state * priv->scroll_to_scale_ratio;
      xrd_window_manager_scale (priv->manager, grab_state, factor,
                                priv->poll_input_rate_ms);
    }

  float y_state = graphene_vec3_get_y (&event->state);
  if (grab_state->window && fabs (y_state) > priv->analog_threshold)
    {
      HoverState *hover_state =
        xrd_window_manager_get_hover_state (priv->manager, controller->index);
      hover_state->distance +=
        priv->scroll_to_push_ratio *
        hover_state->distance *
        graphene_vec3_get_y (&event->state) *
        (priv->poll_input_rate_ms / 1000.);

      XrdPointer *pointer_ray = priv->pointer_ray[controller->index];
      xrd_pointer_set_length (pointer_ray, hover_state->distance);
    }

  g_free (event);
}

static void
_action_grab_cb (OpenVRAction        *action,
                 OpenVRDigitalEvent  *event,
                 XrdClientController *controller)
{
  (void) action;
  XrdClient *self = controller->self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (event->changed)
    {
      if (event->state == 1)
        xrd_window_manager_check_grab (priv->manager, controller->index);
      else
        xrd_window_manager_check_release (priv->manager, controller->index);
    }

  g_free (event);
}

static void
_action_rotate_cb (OpenVRAction        *action,
                   OpenVRAnalogEvent   *event,
                   XrdClientController *controller)
{
  (void) action;
  XrdClient *self = controller->self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  GrabState *grab_state =
      xrd_window_manager_get_grab_state (priv->manager, controller->index);

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

static void
_mark_windows_for_selection_mode (XrdClient *self);

void
_window_grab_start_cb (XrdWindow               *window,
                       XrdControllerIndexEvent *event,
                       gpointer                 _self)
{
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  /* don't grab if this window is already grabbed */
  if (priv->selection_mode)
    {
      gboolean pinned = xrd_window_manager_is_pinned (priv->manager, window);
      xrd_window_manager_set_pin (priv->manager, window, !pinned);
      _mark_windows_for_selection_mode (self);
      return;
    }


  if (xrd_window_manager_is_grabbed (priv->manager, window))
    {
      g_free (event);
      return;
    }

  xrd_window_manager_drag_start (priv->manager, event->index);

  if (event->index == xrd_input_synth_synthing_controller (priv->input_synth))
    xrd_desktop_cursor_hide (priv->cursor);

  g_free (event);
}

void
_window_grab_cb (XrdWindow    *window,
                 XrdGrabEvent *event,
                 gpointer     _self)
{
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  XrdPointerTip *pointer_tip =
    priv->pointer_tip[event->controller_index];
  xrd_pointer_tip_set_transformation_matrix (pointer_tip, &event->pose);

  xrd_pointer_tip_set_constant_width (pointer_tip);
  g_free (event);
}

/* TODO: Move to xrd window */
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

static void
_mark_windows_for_selection_mode (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdWindowManager *manager = xrd_client_get_manager (self);
  if (priv->selection_mode)
    {
      GSList *all = xrd_window_manager_get_windows (manager);
      for (GSList *l = all; l != NULL; l = l->next)
        {
          XrdWindow *win = l->data;

          if (xrd_window_manager_is_pinned (manager, win))
            xrd_window_mark_color (win, 0.0, 0.0, 1.0);
          else
            xrd_window_mark_color (win, 0.1, 0.1, 0.1);

          xrd_window_set_hidden (win, FALSE);
        }
    }
  else
    {
      GSList *all = xrd_window_manager_get_windows (manager);
      for (GSList *l = all; l != NULL; l = l->next)
        {
          XrdWindow *win = l->data;

          xrd_window_unmark (win);

          if (priv->pinned_only &&
              !xrd_window_manager_is_pinned (manager, win))
            xrd_window_set_hidden (win, TRUE);
        }
    }
}

void
_button_hover_cb (XrdWindow     *window,
                  XrdHoverEvent *event,
                  gpointer       _self)
{
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (!priv->selection_mode)
    xrd_window_mark_color (window, .8f, .4f, .2f);

  XrdPointer *pointer =
      priv->pointer_ray[event->controller_index];
  XrdPointerTip *pointer_tip =
      priv->pointer_tip[event->controller_index];

  /* update pointer length and pointer tip */
  graphene_matrix_t window_pose;
  xrd_window_get_transformation (window, &window_pose);

  xrd_pointer_tip_update (pointer_tip, &window_pose, &event->point);
  xrd_pointer_set_length (pointer, event->distance);

  g_free (event);
}

void
_window_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  XrdPointer *pointer_ray = priv->pointer_ray[event->index];
  xrd_pointer_reset_length (pointer_ray);

  /* When leaving this window but now hovering another, the tip should
   * still be active because it is now hovering another window. */
  gboolean active =
      xrd_window_manager_get_hover_state (priv->manager, event->index)->window
          != NULL;

  XrdPointerTip *pointer_tip = priv->pointer_tip[event->index];
  xrd_pointer_tip_set_active (pointer_tip, active);

  XrdInputSynth *input_synth = xrd_client_get_input_synth (self);
  xrd_input_synth_reset_press_state (input_synth);

  if (event->index == xrd_input_synth_synthing_controller (input_synth))
    xrd_desktop_cursor_hide (priv->cursor);

  g_free (event);
}

void
_button_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  /* unmark if no controller is hovering over this button */
  if (!xrd_window_manager_is_hovered (priv->manager, window) &&
      !priv->selection_mode)
    xrd_window_unmark (window);

  _window_hover_end_cb (window, event, _self);

  /* _window_hover_end_cb will free the event */
  /* g_free (event); */
}

void
_button_sphere_press_cb (XrdWindow               *window,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_arrange_sphere (priv->manager);
  g_free (event);
}

void
_button_reset_press_cb (XrdWindow               *window,
                        XrdControllerIndexEvent *event,
                        gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_manager_arrange_reset (priv->manager);
  g_free (event);
}

void
_button_pinned_press_cb (XrdWindow               *button,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) button;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->pinned_only = !priv->pinned_only;
  xrd_client_show_pinned_only (self, priv->pinned_only);
  g_free (event);
}


void
_button_select_pinned_press_cb (XrdOverlayWindow        *button,
                                XrdControllerIndexEvent *event,
                                gpointer                 _self)
{
  (void) event;
  (void) button;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->selection_mode = !priv->selection_mode;
  _mark_windows_for_selection_mode (self);
  g_free (event);
}

gboolean
_init_buttons (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  float button_x = 0.0f;
  graphene_point3d_t position_reset = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_client_add_button (self, &priv->button_reset, "Reset",
                              &position_reset,
                              (GCallback) _button_reset_press_cb,
                              self))
    return FALSE;

  float reset_width_meter =
    xrd_window_get_current_width_meters (priv->button_reset);

  button_x += reset_width_meter;

  graphene_point3d_t position_sphere = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_client_add_button (self, &priv->button_sphere, "Sphere",
                              &position_sphere,
                              (GCallback) _button_sphere_press_cb,
                              self))
    return FALSE;

  button_x += reset_width_meter;

  graphene_point3d_t position_pinned = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_client_add_button (self, &priv->pinned_button,
                              "Pinned",
                              &position_pinned,
                              (GCallback) _button_pinned_press_cb,
                              self))
      return FALSE;

  button_x += reset_width_meter;

  graphene_point3d_t select_pinned = {
    .x =  button_x,
    .y =  0.0f,
    .z = -1.0f
  };
  if (!xrd_client_add_button (self, &priv->select_pinned_button,
                              "Select",
                              &select_pinned,
                              (GCallback) _button_select_pinned_press_cb,
                              self))
      return FALSE;

  return TRUE;
}

static void
_keyboard_press_cb (OpenVRContext *context,
                    GdkEventKey   *event,
                    XrdClient     *self)
{
  (void) context;
  xrd_client_emit_keyboard_press (self, event);

  g_free (event);
}

static void
_keyboard_close_cb (OpenVRContext *context,
                    XrdClient     *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  priv->keyboard_window = NULL;

  g_signal_handler_disconnect(context, priv->keyboard_press_signal);
  g_signal_handler_disconnect(context, priv->keyboard_close_signal);
  priv->keyboard_press_signal = 0;
  priv->keyboard_close_signal = 0;

  g_print ("Keyboard closed\n");
}

static void
_action_show_keyboard_cb (OpenVRAction       *action,
                          OpenVRDigitalEvent *event,
                          XrdClient          *self)
{
  (void) action;
  if (!event->state && event->changed)
    {
      XrdClientPrivate *priv = xrd_client_get_instance_private (self);

      OpenVRContext *context = openvr_context_get_instance ();
      openvr_context_show_system_keyboard (context);

      /* TODO: Perhaps there is a better way to get the window that should
               receive keyboard input */
      int controller = xrd_input_synth_synthing_controller (priv->input_synth);

      priv->keyboard_window = xrd_window_manager_get_hover_state
          (priv->manager, controller)->window;

      priv->keyboard_press_signal =
          g_signal_connect (context, "keyboard-press-event",
                            (GCallback) _keyboard_press_cb, self);
      priv->keyboard_close_signal =
          g_signal_connect (context, "keyboard-close-event",
                            (GCallback) _keyboard_close_cb, self);
    }

  g_free (event);
}

void
_window_hover_cb (XrdWindow     *window,
                  XrdHoverEvent *event,
                  XrdClient     *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  /* update pointer length and pointer tip */
  XrdPointerTip *pointer_tip =
    priv->pointer_tip[event->controller_index];

  graphene_matrix_t window_pose;
  xrd_window_get_transformation (window, &window_pose);
  xrd_pointer_tip_update (pointer_tip, &window_pose, &event->point);

  XrdPointer *pointer = priv->pointer_ray[event->controller_index];
  xrd_pointer_set_length (pointer, event->distance);

  priv->hover_window[event->controller_index] = window;

  if (event->controller_index ==
      xrd_input_synth_synthing_controller (priv->input_synth))
    {
      xrd_input_synth_move_cursor (priv->input_synth, window,
                                   &event->pose, &event->point);

      xrd_desktop_cursor_update (priv->cursor, window, &event->point);

      if (priv->hover_window[event->controller_index] != window)
        xrd_input_synth_reset_scroll (priv->input_synth);
    }

  g_free (event);
}

void
_window_hover_start_cb (XrdWindow               *window,
                        XrdControllerIndexEvent *event,
                        XrdClient               *self)
{
  (void) window;
  (void) event;

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  XrdPointerTip *pointer_tip = priv->pointer_tip[event->index];
  xrd_pointer_tip_set_active (pointer_tip, TRUE);

  g_free (event);
}

void
_manager_no_hover_cb (XrdWindowManager *manager,
                      XrdNoHoverEvent  *event,
                      XrdClient        *self)
{
  (void) manager;

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  XrdPointerTip *pointer_tip =
    priv->pointer_tip[event->controller_index];

  XrdPointer *pointer_ray = priv->pointer_ray[event->controller_index];

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f,
                         0.f,
                         -xrd_pointer_get_default_length (pointer_ray));

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

  xrd_pointer_tip_set_transformation_matrix (pointer_tip, &tip_pose);

  xrd_pointer_tip_set_constant_width (pointer_tip);

  xrd_pointer_tip_set_active (pointer_tip, FALSE);

  xrd_input_synth_reset_scroll (priv->input_synth);

  priv->hover_window[event->controller_index] = NULL;

  g_free (event);
}

static void
_update_input_poll_rate (GSettings *settings, gchar *key, gpointer _self)
{
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (priv->poll_input_source_id != 0)
    g_source_remove (priv->poll_input_source_id);
  priv->poll_input_rate_ms = g_settings_get_int (settings, key);

  priv->poll_input_source_id =
      g_timeout_add (priv->poll_input_rate_ms,
      (GSourceFunc) xrd_client_poll_input_events,
      self);
}

static void
_synth_click_cb (XrdInputSynth *synth,
                 XrdClickEvent *event,
                 XrdClient     *self)
{
  (void) synth;

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (priv->selection_mode)
    return;

  if (priv->hover_window[event->controller_index])
    {
      event->window = priv->hover_window[event->controller_index];
      xrd_client_emit_click (self, event);

      if (event->button == 1)
        {
          XrdWindowManager *manager = xrd_client_get_manager (self);
          HoverState *hover_state =
              xrd_window_manager_get_hover_state
                  (manager, event->controller_index);
          if (hover_state->window != NULL && event->state)
            {
              XrdPointerTip *pointer_tip =
                  priv->pointer_tip[event->controller_index];
              xrd_pointer_tip_animate_pulse (pointer_tip);
            }
        }
    }

  g_free (event);
}

static void
_synth_move_cursor_cb (XrdInputSynth      *synth,
                       XrdMoveCursorEvent *event,
                       XrdClient          *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  if (priv->selection_mode)
    return;

  (void) synth;
  if (!event->ignore)
    xrd_client_emit_move_cursor (self, event);

  g_free (event);
}

void
xrd_client_add_button_callbacks (XrdClient *self,
                                 XrdWindow *button)
{
  g_signal_connect (button, "hover-event",
                    (GCallback) _button_hover_cb, self);

  g_signal_connect (button, "hover-end-event",
                    (GCallback) _button_hover_end_cb, self);
}

void
xrd_client_add_window_callbacks (XrdClient *self,
                                 XrdWindow *window)
{
  g_signal_connect (window, "grab-start-event",
                    (GCallback) _window_grab_start_cb, self);
  g_signal_connect (window, "grab-event",
                    (GCallback) _window_grab_cb, self);
  // g_signal_connect (window, "release-event",
  //                   (GCallback) _window_release_cb, self);
  g_signal_connect (window, "hover-start-event",
                    (GCallback) _window_hover_start_cb, self);
  g_signal_connect (window, "hover-event",
                    (GCallback) _window_hover_cb, self);
  g_signal_connect (window, "hover-end-event",
                    (GCallback) _window_hover_end_cb, self);
}

void
xrd_client_set_pointer (XrdClient  *self,
                        XrdPointer *pointer,
                        uint32_t    id)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->pointer_ray[id] = pointer;
}

void
xrd_client_set_pointer_tip (XrdClient     *self,
                            XrdPointerTip *pointer,
                            uint32_t       id)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->pointer_tip[id] = pointer;
}

void
xrd_client_set_desktop_cursor (XrdClient        *self,
                               XrdDesktopCursor *cursor)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->cursor = cursor;
}

cairo_surface_t*
xrd_client_create_button_surface (unsigned char *image, uint32_t width,
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

static void
xrd_client_init (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "scroll-to-push-ratio",
                                  &priv->scroll_to_push_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "scroll-to-scale-ratio",
                                  &priv->scroll_to_scale_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "analog-threshold", &priv->analog_threshold);

  priv->poll_runtime_event_source_id = 0;
  priv->poll_input_source_id = 0;
  priv->keyboard_window = NULL;
  priv->keyboard_press_signal = 0;
  priv->keyboard_close_signal = 0;
  priv->pinned_only = FALSE;
  priv->selection_mode = FALSE;

  priv->context = openvr_context_get_instance ();
  priv->manager = xrd_window_manager_new ();

  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      priv->hover_window[i] = NULL;
      priv->controllers[i].self = self;
      priv->controllers[i].index = i;
    }

}

static void _system_quit_cb (OpenVRContext *context,
                             GdkEvent      *event,
                             XrdClient     *self)
{
  (void) event;
  /* g_print("Handling VR quit event\n"); */
  openvr_context_acknowledge_quit (context);
  xrd_client_emit_system_quit (self, event);

  g_free (event);
}

void
xrd_client_post_openvr_init (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

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

  priv->wm_actions = openvr_action_set_new_from_url ("/actions/wm");

  priv->input_synth = xrd_input_synth_new ();

  g_signal_connect (priv->context, "quit-event",
                    (GCallback) _system_quit_cb, self);

  _init_buttons (self);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_left",
                             (GCallback) _action_hand_pose_cb,
                             &priv->controllers[0]);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_right",
                             (GCallback) _action_hand_pose_cb,
                             &priv->controllers[1]);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_left",
                             (GCallback) _action_grab_cb,
                             &priv->controllers[0]);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window_right",
                             (GCallback) _action_grab_cb,
                             &priv->controllers[1]);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/rotate_window_left",
                             (GCallback) _action_rotate_cb,
                             &priv->controllers[0]);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/rotate_window_right",
                             (GCallback) _action_rotate_cb,
                             &priv->controllers[1]);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_left",
                             (GCallback) _action_push_pull_scale_cb,
                            &priv->controllers[0]);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale_right",
                             (GCallback) _action_push_pull_scale_cb,
                            &priv->controllers[1]);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_left",
                             (GCallback) _action_push_pull_scale_cb,
                            &priv->controllers[0]);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_right",
                             (GCallback) _action_push_pull_scale_cb,
                            &priv->controllers[1]);

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_left",
                             (GCallback) _action_show_keyboard_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard_right",
                             (GCallback) _action_show_keyboard_cb, self);

  g_signal_connect (priv->manager, "no-hover-event",
                    (GCallback) _manager_no_hover_cb, self);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_input_poll_rate),
                                  "input-poll-rate-ms", self);

  priv->poll_runtime_event_source_id =
      g_timeout_add (20,
                     (GSourceFunc) xrd_client_poll_runtime_events,
                     self);


  g_signal_connect (priv->input_synth, "click-event",
                    (GCallback) _synth_click_cb, self);
  g_signal_connect (priv->input_synth, "move-cursor-event",
                    (GCallback) _synth_move_cursor_cb, self);
}
