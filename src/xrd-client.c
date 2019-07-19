/*
 * xrdesktop
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
#include "xrd-controller.h"

#include "xrd-scene-client.h"
#include "xrd-overlay-client.h"

#include "xrd-container.h"

#include "xrd-math.h"

#include "xrd-button.h"

#include "xrd-client-menu.h"

#define WINDOW_MIN_DIST .05f
#define WINDOW_MAX_DIST 15.f

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

  gboolean pinned_only;
  gboolean selection_mode;
  gboolean ignore_input;

  XrdWindow *keyboard_window;

  gulong keyboard_press_signal;
  gulong keyboard_close_signal;

  guint poll_runtime_event_source_id;
  guint poll_input_source_id;
  guint poll_input_rate_ms;

  double analog_threshold;

  double scroll_to_push_ratio;
  double scroll_to_scale_ratio;

  double pixel_per_meter;

  XrdDesktopCursor *cursor;

  VkImageLayout upload_layout;
  GHashTable *controllers;

  XrdContainer *wm_control_container;

  gint64 last_poll_timestamp;

  gboolean always_show_overlay_pointer;

  /* maps a key to desktop #XrdWindows, but not buttons. */
  GHashTable *window_mapping;

  XrdClientMenu *menu;
} XrdClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XrdClient, xrd_client, G_TYPE_OBJECT)

static void
xrd_client_finalize (GObject *gobject);

gboolean
_init_buttons (XrdClient *self, XrdController *controller);

static void
_destroy_buttons (XrdClient *self);

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
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

void
xrd_client_set_upload_layout (XrdClient *self, VkImageLayout layout)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->upload_layout = layout;
}

VkImageLayout
xrd_client_get_upload_layout (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->upload_layout;
}

/**
 * xrd_client_add_container:
 * @self: The #XrdClient
 * @container: The #XrdContainer to add
 *
 * For a container to start behaving according to its layout and attachment,
 * it must be added to the client.
 *
 * Note: windows in the container must be added to the client separately with
 * xrd_client_add_window(), preferably with draggable set to false.
 */
void
xrd_client_add_container (XrdClient *self,
                          XrdContainer *container)
{
  XrdWindowManager *manager = xrd_client_get_manager (self);
  xrd_window_manager_add_container (manager, container);
}

void
xrd_client_remove_container (XrdClient *self,
                             XrdContainer *container)
{
  XrdWindowManager *manager = xrd_client_get_manager (self);
  xrd_window_manager_remove_container (manager, container);
}

/**
 * xrd_client_add_window:
 * @self: The #XrdClient
 * @window: The #XrdWindow to add
 * @draggable: Desktop windows should set this to true. This will enable the
 * expected interaction of being able to grab windows and drag them around.
 * It should be set to false for example for
 *  - child windows
 *  - windows in a container that is attached to the FOV, a controller, etc.
 * @lookup_key: If looking up the #XrdWindow by a key with
 * xrd_client_lookup_window() should be enabled, set to != NULL.
 * Note that an #XrdWindow can be replaced by the overlay-scene switch.
 * Therefore the #XrdWindow should always be looked up instead of cached.
 */
void
xrd_client_add_window (XrdClient *self,
                       XrdWindow *window,
                       gboolean   draggable,
                       gpointer lookup_key)
{
  XrdWindowFlags flags = XRD_WINDOW_HOVERABLE | XRD_WINDOW_DESTROY_WITH_PARENT;

  /* User can't drag child windows, they are attached to the parent.
   * The child window's position is managed by its parent, not the WM. */
  if (draggable)
    flags |= XRD_WINDOW_DRAGGABLE | XRD_WINDOW_MANAGED;

  XrdWindowManager *manager = xrd_client_get_manager (self);

  xrd_window_manager_add_window (manager, window, flags);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  if (priv->pinned_only &&
      !(flags & XRD_WINDOW_BUTTON) &&
      !xrd_window_is_pinned (window))
    {
      xrd_window_hide (window);
    }

  xrd_client_add_window_callbacks (self, window);

  if (lookup_key != NULL)
    g_hash_table_insert (priv->window_mapping, lookup_key,
                         xrd_window_get_data (window));
}

XrdWindow *
xrd_client_lookup_window (XrdClient *self,
                          gpointer key)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdWindowData *data = g_hash_table_lookup (priv->window_mapping, key);
  if (!data)
    {
      g_print ("Error looking up window %p\n", key);
      return NULL;
    }
  return data->xrd_window;
}

/**
 * xrd_client_button_new_from_text:
 * @self: The #XrdClient
 * @width: Width in meters.
 * @height: Height in meters.
 * @ppm: Density in pixels per meter
 * @label_count: The number of text lines given in @label
 * @label: One or more lines of text that will be displayed on the button.
 *
 * Creates a button and submits a Cairo rendered text label to it.
 */

XrdWindow*
xrd_client_button_new_from_text (XrdClient *self,
                                 float      width,
                                 float      height,
                                 float      ppm,
                                 int        label_count,
                                 gchar    **label)
{
  GString *full_label = g_string_new ("");
  for (int i = 0; i < label_count; i++)
    {
      g_string_append (full_label, label[i]);
      if (i < label_count - 1)
        g_string_append (full_label, " ");
    }

  XrdWindow *button =
    xrd_client_window_new_from_meters (self, full_label->str,
                                       width, height, ppm);

  g_string_free (full_label, FALSE);

  if (button == NULL)
    {
      g_printerr ("Could not create button.\n");
      return NULL;
    }

  GulkanClient *gc = xrd_client_get_uploader (self);
  VkImageLayout layout = xrd_client_get_upload_layout (self);

  xrd_button_set_text (button, gc, layout, label_count, label);

  return button;
}

XrdWindow*
xrd_client_button_new_from_icon (XrdClient   *self,
                                 float        width,
                                 float        height,
                                 float        ppm,
                                 const gchar *url)
{
  XrdWindow *button = xrd_client_window_new_from_meters (self, url,
                                                         width, height, ppm);

  if (button == NULL)
    {
      g_printerr ("Could not create button.\n");
      return NULL;
    }

  GulkanClient *gc = xrd_client_get_uploader (self);
  VkImageLayout layout = xrd_client_get_upload_layout (self);

  xrd_button_set_icon (button, gc, layout, url);

  return button;
}

/**
 * xrd_client_add_button:
 * @self: The #XrdClient
 * @button: The button (#XrdWindow) to add.
 * @position: World space position of the button.
 * @press_callback: A function that will be called when the button is grabbed.
 * @press_callback_data: User pointer passed to @press_callback.
 *
 * Buttons are special windows that can not be grabbed and dragged around.
 * Instead a button's press_callback is called on the grab action.
 */
void
xrd_client_add_button (XrdClient          *self,
                       XrdWindow          *button,
                       graphene_point3d_t *position,
                       GCallback           press_callback,
                       gpointer            press_callback_data)
{
  graphene_matrix_t transform;
  graphene_matrix_init_translate (&transform, position);
  xrd_window_set_transformation (button, &transform);

  XrdWindowManager *manager = xrd_client_get_manager (self);
  xrd_window_manager_add_window (manager,
                                 button,
                                 XRD_WINDOW_HOVERABLE |
                                 XRD_WINDOW_DESTROY_WITH_PARENT |
                                 XRD_WINDOW_BUTTON);

  g_signal_connect (button, "grab-start-event",
                    (GCallback) press_callback, press_callback_data);

  xrd_client_add_button_callbacks (self, button);
}

void
xrd_client_set_pin (XrdClient *self,
                    XrdWindow *win,
                    gboolean pin)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  xrd_window_set_pin (win, pin, priv->pinned_only);
}

void
xrd_client_show_pinned_only (XrdClient *self,
                             gboolean pinned_only)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->pinned_only = pinned_only;

  GSList *windows = xrd_window_manager_get_windows (priv->manager);

  for (GSList *l = windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;
      gboolean pinned = xrd_window_is_pinned (window);
      if (!pinned_only || (pinned_only && pinned))
        xrd_window_show (window);
      else
        xrd_window_hide (window);
    }
}

static void
_device_activate_cb (OpenVRContext          *context,
                     OpenVRDeviceIndexEvent *event,
                     gpointer               _self);

static void
_activate_controller (XrdClient *self,
                      guint64 controller_handle)
{
  OpenVRContext *context = openvr_context_get_instance ();
  OpenVRDeviceIndexEvent event = {
    .controller_handle = controller_handle
  };
  _device_activate_cb (context, &event, self);
}

static XrdController *
_lookup_controller (XrdClient *self,
                    guint64    controller_handle)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = g_hash_table_lookup (priv->controllers,
                                                   &controller_handle);
  return controller;
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

void
xrd_client_init_controller (XrdClient *self,
                            XrdController *controller)
{
  XrdClientClass *klass = XRD_CLIENT_GET_CLASS (self);
  if (klass->init_controller == NULL)
      return;
  klass->init_controller (self, controller);
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

  if (!priv->input_synth)
    {
      g_print ("Error: No window hovered because synth is NULL\n");
      return NULL;
    }

  guint64 controller_handle =
    xrd_input_synth_synthing_controller (priv->input_synth);
  XrdController *controller = _lookup_controller (self, controller_handle);

  /* no controller, nothing hovered */
  if (controller == NULL)
    return NULL;

  XrdWindow *parent = xrd_controller_get_hover_state (controller)->window;
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
                             OpenVRQuitEvent *event)
{
  g_signal_emit (self, signals[REQUEST_QUIT_EVENT], 0, event);
}

XrdClient *
xrd_client_new (void)
{
  return (XrdClient*) g_object_new (XRD_TYPE_CLIENT, 0);
}

GSList *
xrd_client_get_windows (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return xrd_window_manager_get_windows (priv->manager);
}

static void
xrd_client_finalize (GObject *gobject)
{
  XrdClient *self = XRD_CLIENT (gobject);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (priv->wm_control_container)
    _destroy_buttons (self);

  if (priv->poll_runtime_event_source_id > 0)
    g_source_remove (priv->poll_runtime_event_source_id);
  if (priv->poll_input_source_id > 0)
    g_source_remove (priv->poll_input_source_id);

  g_object_unref (priv->manager);
  g_clear_object (&priv->wm_actions);

  /* hash table unref also destroysControllers */
  g_hash_table_unref (priv->controllers);

  g_clear_object (&priv->cursor);
  g_clear_object (&priv->input_synth);

  g_clear_object (&priv->context);
  g_clear_object (&priv->wm_control_container);

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

static gboolean
_match_value_ptr (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  (void) key;
  return value == user_data;
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

  g_hash_table_foreach_remove (priv->window_mapping, _match_value_ptr,
                               xrd_window_get_data (window));

  xrd_window_manager_remove_window (priv->manager, window);

  GList *controllers = g_hash_table_get_values (priv->controllers);
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      if (xrd_controller_get_hover_state (controller)->window ==
          XRD_WINDOW (window))
        {
          XrdControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_end_event->controller_handle =
            xrd_controller_get_handle (controller);
          xrd_window_emit_hover_end (window, hover_end_event);

          xrd_controller_reset_hover_state (controller);
        }

      if (xrd_controller_get_grab_state (controller)->window ==
          XRD_WINDOW (window))
        xrd_controller_reset_grab_state (controller);
    }
  g_list_free(controllers);
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
    {
      g_printerr ("Error polling events: No OpenVR Context\n");
      priv->poll_input_source_id = 0;
      return FALSE;
    }

  if (!openvr_action_set_poll (priv->wm_actions))
    {
      g_printerr ("Error polling wm actions\n");
      priv->poll_input_source_id = 0;
      return FALSE;
    }

  if (xrd_client_is_hovering (self) && !xrd_client_is_grabbing (self) &&
      !xrd_input_synth_poll_events (priv->input_synth))
    {
      g_printerr ("Error polling synth actions\n");
      priv->poll_input_source_id = 0;
      return FALSE;
    }

  xrd_window_manager_poll_window_events (priv->manager);

  priv->last_poll_timestamp = g_get_monotonic_time ();
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
                      XrdClient               *self)
{
  (void) action;
  if (!event->device_connected || !event->valid || !event->active)
    {
      g_free (event);
      return;
    }

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    {
      g_print ("Pose callback: activating %lu\n", event->controller_handle);
      _activate_controller (self, event->controller_handle);
      controller = _lookup_controller (self, event->controller_handle);
    }


  if (controller == NULL)
    {
      g_free (event);
      return;
    }

  xrd_window_manager_update_pose (priv->manager, &event->pose, controller);

  xrd_pointer_move (xrd_controller_get_pointer (controller), &event->pose);

  XrdWindow *hovered_window =
    xrd_controller_get_hover_state (controller)->window;
  GSList *buttons = xrd_window_manager_get_buttons (priv->manager);

  gboolean hovering_window_for_input =
    hovered_window != NULL &&
    g_slist_find (buttons, hovered_window) == NULL;

  /* show cursor while synth controller hovers window, but doesn't grab */
  if (xrd_controller_get_handle (controller) ==
          xrd_input_synth_synthing_controller (priv->input_synth) &&
      hovering_window_for_input &&
      xrd_controller_get_grab_state (controller)->window == NULL)
    xrd_desktop_cursor_show (priv->cursor);

  g_free (event);
}

static void
_action_hand_pose_hand_grip_cb (OpenVRAction    *action,
                                OpenVRPoseEvent *event,
                                XrdClient       *self)
{
  (void) action;
  if (!event->device_connected || !event->valid || !event->active)
    {
      g_free (event);
      return;
    }

  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  if (controller == NULL)
    {
      g_free (event);
      return;
    }

  xrd_controller_update_pose_hand_grip (controller, &event->pose);
  g_free (event);
}


static void
_perform_push_pull (XrdClient *self,
                    XrdController *controller,
                    float push_pull_strength,
                    float ms_since_last_poll)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  XrdHoverState *hover_state = xrd_controller_get_hover_state (controller);

  float new_dist =
    hover_state->distance +

    hover_state->distance *
    (float) priv->scroll_to_push_ratio *
    push_pull_strength *

    (ms_since_last_poll / 1000.f);

  if (new_dist < WINDOW_MIN_DIST || new_dist > WINDOW_MAX_DIST)
    return;

  hover_state->distance = new_dist;

  xrd_pointer_set_length (xrd_controller_get_pointer (controller),
                          hover_state->distance);
}

static void
_action_push_pull_scale_cb (OpenVRAction        *action,
                            OpenVRAnalogEvent   *event,
                            XrdClient           *self)
{
  (void) action;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    {
      g_free (event);
      return;
    }

  float ms_since_last_poll =
    (g_get_monotonic_time () - priv->last_poll_timestamp) / 1000.f;

  XrdGrabState *grab_state = xrd_controller_get_grab_state (controller);

  double x_state = (double) graphene_vec3_get_x (&event->state);
  double y_state = (double) graphene_vec3_get_y (&event->state);

  /* go back to undecided when "stopping" current action,
   * to allow switching actions without letting go of the window. */
  if (fabs (x_state) < priv->analog_threshold &&
      fabs (y_state) < priv->analog_threshold)
    {
      grab_state->transform_lock = XRD_TRANSFORM_LOCK_NONE;
      g_free (event);
      return;
    }

  if (grab_state->transform_lock == XRD_TRANSFORM_LOCK_NONE)
    {
      if (fabs (x_state) > fabs (y_state) &&
          fabs (x_state) > priv->analog_threshold)
        grab_state->transform_lock = XRD_TRANSFORM_LOCK_SCALE;

      else if (fabs (y_state) > fabs (x_state) &&
          fabs (y_state) > priv->analog_threshold)
        grab_state->transform_lock = XRD_TRANSFORM_LOCK_PUSH_PULL;
    }

  if (grab_state->transform_lock == XRD_TRANSFORM_LOCK_SCALE)
    {
      double factor = x_state * priv->scroll_to_scale_ratio;
      xrd_window_manager_scale (priv->manager, grab_state, (float) factor,
                                ms_since_last_poll);
    }
  else if (grab_state->transform_lock == XRD_TRANSFORM_LOCK_PUSH_PULL)
    _perform_push_pull (self, controller, graphene_vec3_get_y (&event->state),
                        ms_since_last_poll);

  g_free (event);
}

static void
_action_push_pull_cb (OpenVRAction        *action,
                      OpenVRAnalogEvent   *event,
                      XrdClient           *self)
{
  (void) action;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    {
      g_free (event);
      return;
    }

  float ms_since_last_poll =
    (g_get_monotonic_time () - priv->last_poll_timestamp) / 1000.f;

  XrdGrabState *grab_state = xrd_controller_get_grab_state (controller);

  double y_state = (double) graphene_vec3_get_y (&event->state);
  if (grab_state->window && fabs (y_state) > priv->analog_threshold)
    _perform_push_pull (self, controller, graphene_vec3_get_y (&event->state),
                        ms_since_last_poll);

  g_free (event);
}

static void
_action_grab_cb (OpenVRAction        *action,
                 OpenVRDigitalEvent  *event,
                 XrdClient           *self)
{
  (void) action;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    return;

  if (event->changed)
    {
      if (event->state == 1)
        xrd_window_manager_check_grab (priv->manager, controller);
      else
        xrd_window_manager_check_release (priv->manager, controller);
    }

  g_free (event);
}

typedef struct {
  XrdGrabState *grab_state;
  graphene_quaternion_t from;
  graphene_quaternion_t from_neg;
  graphene_quaternion_t to;
  float interpolate;
  gint64 last_timestamp;
} XrdOrientationTransition;

static gboolean
_interpolate_orientation_cb (gpointer _transition)
{
  XrdOrientationTransition *transition =
    (XrdOrientationTransition*) _transition;

  XrdGrabState *grab_state = transition->grab_state;

  graphene_quaternion_slerp (&transition->from,
                             &transition->to,
                             transition->interpolate,
                             &grab_state->window_rotation);

  graphene_quaternion_slerp (&transition->from_neg,
                             &transition->to,
                             transition->interpolate,
                             &grab_state->inverse_controller_rotation);

  gint64 now = g_get_monotonic_time ();
  float ms_since_last = (now - transition->last_timestamp) / 1000.f;
  transition->last_timestamp = now;

  /* in seconds */
  const float transition_duration = 0.2f;

  transition->interpolate += ms_since_last / 1000.f / transition_duration;

  if (transition->interpolate > 1)
    {
      graphene_quaternion_init_identity (&grab_state->inverse_controller_rotation);
      graphene_quaternion_init_identity (&grab_state->window_rotation);
      g_free (transition);
      return FALSE;
    }

  return TRUE;
}

static void
_action_reset_orientation_cb (OpenVRAction       *action,
                              OpenVRDigitalEvent *event,
                              XrdClient          *self)
{
  (void) action;

  if (!(event->changed && event->state == 1))
    return;

  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    return;

  XrdGrabState *grab_state = xrd_controller_get_grab_state (controller);
  if (grab_state->window == NULL)
    return;

  XrdOrientationTransition *transition =
    g_malloc (sizeof (XrdOrientationTransition));

  /* TODO: Check if animation is already in progress */

  transition->last_timestamp = g_get_monotonic_time ();
  transition->interpolate = 0.;
  transition->grab_state = grab_state;

  graphene_quaternion_init_identity (&transition->to);
  graphene_quaternion_init_from_quaternion (&transition->from,
                                            &grab_state->window_rotation);
  graphene_quaternion_init_from_quaternion (&transition->from_neg,
                                            &grab_state->inverse_controller_rotation);

  g_timeout_add (10, _interpolate_orientation_cb, transition);

  g_free (event);
}

static void
_mark_windows_for_selection_mode (XrdClient *self);

static void
_window_grab_start_cb (XrdWindow               *window,
                       XrdControllerIndexEvent *event,
                       gpointer                 _self)
{
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  if (priv->selection_mode)
    {
      gboolean pinned = xrd_window_is_pinned (window);
      /* in selection mode, windows are always visible */
      xrd_window_set_pin (window, !pinned, FALSE);
      _mark_windows_for_selection_mode (self);
      g_free (event);
      return;
    }

  /* don't grab if this window is already grabbed */
  if (xrd_client_is_grabbed (self, window))
    {
      g_free (event);
      return;
    }

  xrd_window_manager_drag_start (priv->manager, controller);

  if (event->controller_handle ==
      xrd_input_synth_synthing_controller (priv->input_synth))
    xrd_desktop_cursor_hide (priv->cursor);

  g_free (event);
}

static void
_window_grab_cb (XrdWindow    *window,
                 XrdGrabEvent *event,
                 gpointer     _self)
{
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  xrd_pointer_tip_set_transformation (
    xrd_controller_get_pointer_tip (controller), &event->pose);

  xrd_pointer_tip_update_apparent_size (
    xrd_controller_get_pointer_tip (controller));
  g_free (event);
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

          if (xrd_window_is_pinned (win))
            xrd_window_select (win);
          else
            xrd_window_deselect (win);

          xrd_window_show (win);
        }
    }
  else
    {
      GSList *all = xrd_window_manager_get_windows (manager);
      for (GSList *l = all; l != NULL; l = l->next)
        {
          XrdWindow *win = l->data;

          xrd_window_end_selection (win);
          if (priv->pinned_only)
            {
              if (xrd_window_is_pinned (win))
                xrd_window_show (win);
              else
                xrd_window_hide (win);
            }
        }
    }
}

static void
_button_hover_cb (XrdWindow     *window,
                  XrdHoverEvent *event,
                  gpointer       _self)
{
  XrdClient *self = XRD_CLIENT (_self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  xrd_window_select (window);

  XrdPointer *pointer = xrd_controller_get_pointer (controller);
  XrdPointerTip *pointer_tip = xrd_controller_get_pointer_tip (controller);

  /* update pointer length and pointer tip */
  graphene_matrix_t window_pose;
  xrd_window_get_transformation (window, &window_pose);

  xrd_pointer_tip_update (pointer_tip, &window_pose, &event->point);
  xrd_pointer_set_length (pointer, event->distance);

  g_free (event);
}

static void
_window_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  (void) window;
  XrdClient *self = XRD_CLIENT (_self);
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  xrd_pointer_reset_length (xrd_controller_get_pointer (controller));

  /* When leaving this window but now hovering another, the tip should
   * still be active because it is now hovering another window. */
  gboolean active = xrd_controller_get_hover_state (controller)->window != NULL;
  xrd_pointer_tip_set_active (xrd_controller_get_pointer_tip (controller),
                              active);

  if (!priv->always_show_overlay_pointer &&
      !active && XRD_IS_OVERLAY_CLIENT (self))
    {
      xrd_pointer_hide (xrd_controller_get_pointer (controller));
      xrd_pointer_tip_hide (xrd_controller_get_pointer_tip (controller));
    }

  XrdInputSynth *input_synth = xrd_client_get_input_synth (self);
  xrd_input_synth_reset_press_state (input_synth);

  if (event->controller_handle ==
      xrd_input_synth_synthing_controller (input_synth))
    xrd_desktop_cursor_hide (priv->cursor);

  g_free (event);
}

static void
_button_hover_end_cb (XrdWindow               *window,
                      XrdControllerIndexEvent *event,
                      gpointer                 _self)
{
  (void) event;
  XrdClient *self = XRD_CLIENT (_self);
  //XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  /* unmark if no controller is hovering over this button */
  if (!xrd_client_is_hovered (self, window))
    xrd_window_end_selection (window);

  _window_hover_end_cb (window, event, _self);

  /* _window_hover_end_cb will free the event */
  /* g_free (event); */
}

static void
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

static void
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

static void
_button_pinned_press_cb (XrdWindow               *button,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) button;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (priv->selection_mode)
    return;

  xrd_client_menu_toggle_button (priv->menu, button, !priv->pinned_only);
  xrd_client_show_pinned_only (self, !priv->pinned_only);
  g_free (event);
}


static void
_button_select_pinned_press_cb (XrdWindow               *button,
                                XrdControllerIndexEvent *event,
                                gpointer                 _self)
{
  (void) event;
  (void) button;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  priv->selection_mode = !priv->selection_mode;
  xrd_client_menu_toggle_button (priv->menu, button, priv->selection_mode);

  _mark_windows_for_selection_mode (self);

  g_free (event);
}

static void
_button_ignore_input_press_cb (XrdWindow               *button,
                               XrdControllerIndexEvent *event,
                               gpointer                 _self)
{
  (void) event;
  (void) button;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  priv->ignore_input = !priv->ignore_input;
  xrd_client_menu_toggle_button (priv->menu, button, priv->ignore_input);

  xrd_window_manager_set_hover_mode (priv->manager,
                                     priv->ignore_input ?
                                         XRD_HOVER_MODE_BUTTONS :
                                         XRD_HOVER_MODE_EVERYTHING);
  g_free (event);
}

gboolean
_init_buttons (XrdClient *self, XrdController *controller)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  /* Use head attached menu if less than 2 controllers are active.
   * Controller attached requires more than 1 controller to use. */
  int attach_controller = g_hash_table_size (priv->controllers) > 1;

  priv->menu = xrd_client_menu_new ();
  xrd_client_menu_initialize (priv->menu, self,
                              attach_controller ?
                                  XRD_CONTAINER_ATTACHMENT_HAND :
                                  XRD_CONTAINER_ATTACHMENT_HEAD,
                              3, 2, controller);

  XrdWindow *button =
    xrd_client_menu_create_button (priv->menu, XRD_BUTTON_ICON, 0, 0,
                                   "/icons/align-sphere-symbolic.svg",
                                   (GCallback) _button_sphere_press_cb);

  button =
    xrd_client_menu_create_button (priv->menu, XRD_BUTTON_ICON, 0, 1,
                                   "/icons/edit-undo-symbolic.svg",
                                   (GCallback) _button_reset_press_cb);

  button =
    xrd_client_menu_create_button (priv->menu, XRD_BUTTON_ICON, 1, 0,
                                   "/icons/view-pin-symbolic.svg",
                                   (GCallback) _button_select_pinned_press_cb);
  xrd_client_menu_set_button_toggleable (priv->menu, button,
                                         "/icons/object-select-symbolic.svg",
                                         priv->selection_mode);

  button =
    xrd_client_menu_create_button (priv->menu, XRD_BUTTON_ICON, 1, 1,
                                   "/icons/object-visible-symbolic.svg",
                                   (GCallback) _button_pinned_press_cb);
  xrd_client_menu_set_button_toggleable (priv->menu, button,
                                         "/icons/object-hidden-symbolic.svg",
                                         priv->pinned_only);


  button =
    xrd_client_menu_create_button (priv->menu, XRD_BUTTON_ICON, 2, 0.5f,
                                   "/icons/input-mouse-symbolic.svg",
                                   (GCallback) _button_ignore_input_press_cb);
  xrd_client_menu_set_button_toggleable (priv->menu, button,
                                         "/icons/input-no-mouse-symbolic.svg",
                                         priv->ignore_input);

  return TRUE;
}

static void
_destroy_buttons (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  g_clear_object (&priv->menu);
}

static void
_keyboard_press_cb (OpenVRContext *context,
                    GdkEventKey   *event,
                    XrdClient     *self)
{
  (void) context;
  xrd_client_emit_keyboard_press (self, event);

  /* TODO: this crashes
  gdk_event_free ((GdkEvent*) event); */
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

      guint64 controller_handle =
        xrd_input_synth_synthing_controller (priv->input_synth);
      XrdController *controller = _lookup_controller (self, controller_handle);

      /* window hovered by the synthing controller receives input */
      priv->keyboard_window =
        xrd_controller_get_hover_state (controller)->window;

      priv->keyboard_press_signal =
          g_signal_connect (context, "keyboard-press-event",
                            (GCallback) _keyboard_press_cb, self);
      priv->keyboard_close_signal =
          g_signal_connect (context, "keyboard-close-event",
                            (GCallback) _keyboard_close_cb, self);
    }

  g_free (event);
}

static void
_window_hover_cb (XrdWindow     *window,
                  XrdHoverEvent *event,
                  XrdClient     *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  graphene_matrix_t window_pose;
  xrd_window_get_transformation_no_scale (window, &window_pose);
  xrd_pointer_tip_update (xrd_controller_get_pointer_tip (controller),
                          &window_pose, &event->point);

  xrd_pointer_set_length (xrd_controller_get_pointer (controller),
                          event->distance);

  xrd_controller_get_hover_state (controller)->window = window;

  if (event->controller_handle ==
      xrd_input_synth_synthing_controller (priv->input_synth))
    {
      xrd_input_synth_move_cursor (priv->input_synth, window,
                                   &event->pose, &event->point);

      xrd_desktop_cursor_update (priv->cursor, window, &event->point);

      if (xrd_controller_get_hover_state (controller)->window != window)
        xrd_input_synth_reset_scroll (priv->input_synth);
    }

  g_free (event);
}

static void
_window_hover_start_cb (XrdWindow               *window,
                        XrdControllerIndexEvent *event,
                        XrdClient               *self)
{
  (void) window;
  (void) event;

  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  xrd_pointer_tip_set_active (xrd_controller_get_pointer_tip (controller),
                              TRUE);

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  /* not necessary for scene because there pointer is always shown. */
  if (!priv->always_show_overlay_pointer && !XRD_IS_SCENE_CLIENT (self))
    {
      xrd_pointer_show (xrd_controller_get_pointer (controller));
      xrd_pointer_tip_show (xrd_controller_get_pointer_tip (controller));
    }

  g_free (event);
}

static void
_manager_no_hover_cb (XrdWindowManager *manager,
                      XrdNoHoverEvent  *event,
                      XrdClient        *self)
{
  (void) manager;

  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);

  XrdPointerTip *pointer_tip = xrd_controller_get_pointer_tip (controller);

  XrdPointer *pointer_ray = xrd_controller_get_pointer (controller);

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

  xrd_pointer_tip_set_transformation (pointer_tip, &tip_pose);

  xrd_pointer_tip_update_apparent_size (pointer_tip);

  xrd_pointer_tip_set_active (pointer_tip, FALSE);

  if (xrd_input_synth_synthing_controller (priv->input_synth) ==
      event->controller_handle)
    xrd_input_synth_reset_scroll (priv->input_synth);

  xrd_controller_reset_hover_state (controller);

  g_free (event);
}

static void
_update_input_poll_rate (GSettings *settings, gchar *key, gpointer _self)
{
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  if (priv->poll_input_source_id != 0)
    g_source_remove (priv->poll_input_source_id);
  priv->poll_input_rate_ms = g_settings_get_uint (settings, key);

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
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (priv->selection_mode)
    return;

  if (xrd_controller_get_hover_state (controller)->window)
    {
      event->window = xrd_controller_get_hover_state (controller)->window;
      xrd_client_emit_click (self, event);

      if (event->button == 1)
        {
          if (xrd_controller_get_hover_state (controller)->window != NULL &&
              event->state)
            {
              xrd_pointer_tip_animate_pulse (
                xrd_controller_get_pointer_tip (controller));
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

static void
_action_menu_cb (OpenVRAction        *action,
                 OpenVRDigitalEvent  *event,
                 XrdClient           *self)
{
  (void) action;
  XrdController *controller = _lookup_controller (self,
                                                  event->controller_handle);
  if (controller == NULL)
    return;

  if (event->changed && event->state == 1 &&
      !xrd_controller_get_hover_state (controller)->window)
    {
      XrdClientPrivate *priv = xrd_client_get_instance_private (self);
      if (priv->menu)
        _destroy_buttons (self);
      else
        _init_buttons (self, controller);

    }
  g_free (event);
}


XrdWindow *
xrd_client_window_new_from_meters (XrdClient  *client,
                                   const char *title,
                                   float       width,
                                   float       height,
                                   float       ppm)
{
  XrdWindow *window;
  if (XRD_IS_SCENE_CLIENT (client))
    {
      window = XRD_WINDOW (xrd_scene_window_new_from_meters (title, width,
                                                             height, ppm));
      xrd_scene_window_initialize (XRD_SCENE_WINDOW (window));
    }
  else
    {
      window = XRD_WINDOW (xrd_overlay_window_new_from_meters (title, width,
                                                               height, ppm));
    }
  return window;
}

static XrdWindow *
xrd_client_window_new_from_data (XrdClient  *client,
                                 XrdWindowData *data)
{
  XrdWindow *window;
  if (XRD_IS_SCENE_CLIENT (client))
    {
      window = XRD_WINDOW (xrd_scene_window_new_from_data (data));
      xrd_scene_window_initialize (XRD_SCENE_WINDOW (window));
    }
  else
    {
      window = XRD_WINDOW (xrd_overlay_window_new_from_data (data));
    }
  return window;
}

XrdWindow *
xrd_client_window_new_from_pixels (XrdClient  *client,
                                   const char *title,
                                   uint32_t    width,
                                   uint32_t    height,
                                   float       ppm)
{
  XrdWindow *window;
  if (XRD_IS_SCENE_CLIENT (client))
    {
      window = XRD_WINDOW (xrd_scene_window_new_from_pixels (title, width,
                                                             height, ppm));
      xrd_scene_window_initialize (XRD_SCENE_WINDOW (window));
    }
  else
    {
      window = XRD_WINDOW (xrd_overlay_window_new_from_pixels (title, width,
                                                               height, ppm));
    }
  return window;
}

XrdWindow *
xrd_client_window_new_from_native (XrdClient   *client,
                                   const gchar *title,
                                   gpointer     native,
                                   uint32_t     width_pixels,
                                   uint32_t     height_pixels,
                                   float        ppm)
{
  XrdWindow *window;
  if (XRD_IS_SCENE_CLIENT (client))
    {
      window = XRD_WINDOW (xrd_scene_window_new_from_native (title, native,
                                                             width_pixels,
                                                             height_pixels,
                                                             ppm));
      xrd_scene_window_initialize (XRD_SCENE_WINDOW (window));
    }
  else
    {
      window = XRD_WINDOW (xrd_overlay_window_new_from_native (title, native,
                                                               width_pixels,
                                                               height_pixels,
                                                               ppm));
    }
  return window;
}

void
xrd_client_add_button_callbacks (XrdClient *self,
                                 XrdWindow *button)
{
  g_signal_connect (button, "hover-event",
                    (GCallback) _button_hover_cb, self);

  g_signal_connect (button, "hover-start-event",
                    (GCallback) _window_hover_start_cb, self);

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
xrd_client_set_desktop_cursor (XrdClient        *self,
                               XrdDesktopCursor *cursor)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  priv->cursor = cursor;
  xrd_desktop_cursor_hide (priv->cursor);
}

XrdDesktopCursor*
xrd_client_get_desktop_cursor (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->cursor;
}

GHashTable *
xrd_client_get_controllers (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  return priv->controllers;
}

static void
_device_activate_cb (OpenVRContext          *context,
                     OpenVRDeviceIndexEvent *event,
                     gpointer               _self)
{
  (void) context;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  guint64 handle = event->controller_handle;

  if (g_hash_table_contains (priv->controllers, &handle))
    {
      g_print ("Controller %lu already active\n", handle);
      return;
    }

  g_print ("Controller %lu activated.\n", handle);
  XrdController *controller = xrd_controller_new (handle);

  guint64 *key = g_malloc (sizeof (guint64));
  *key = handle;
  g_hash_table_insert (priv->controllers, key, controller);

  xrd_client_init_controller (self, controller);

  if (g_hash_table_size (priv->controllers) == 1)
    xrd_input_synth_hand_off_to_controller (priv->input_synth, handle);

  if (!priv->always_show_overlay_pointer && XRD_IS_OVERLAY_CLIENT (self))
    {
       xrd_pointer_hide (xrd_controller_get_pointer (controller));
       xrd_pointer_tip_hide (xrd_controller_get_pointer_tip (controller));
    }
}

static void
_device_deactivate_cb (OpenVRContext          *context,
                       OpenVRDeviceIndexEvent *event,
                       gpointer               _self)
{
  (void) context;
  XrdClient *self = _self;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  guint64 handle = event->controller_handle;
  g_print ("Controller %lu deactivated..\n", handle);

  /* hashmap destroys key & val
  g_object_unref (controller); */

  g_hash_table_remove (priv->controllers, &handle);

  if (xrd_input_synth_synthing_controller (priv->input_synth) == handle &&
      g_hash_table_size (priv->controllers) > 0)
    {
      GList *controllers = g_hash_table_get_values (priv->controllers);
      XrdController *controller = XRD_CONTROLLER (controllers->data);
      xrd_input_synth_hand_off_to_controller (
        priv->input_synth, xrd_controller_get_handle (controller));
      g_list_free (controllers);
    }

}

static void
_update_show_overlay_pointer (GSettings *settings, gchar *key, gpointer _data)
{
  XrdClient *self = _data;
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  gboolean val = g_settings_get_boolean (settings, key);
  priv->always_show_overlay_pointer = val;

  if (XRD_IS_SCENE_CLIENT (self))
    return;

  GHashTable *controller_table = xrd_client_get_controllers (self);
  GList *controllers = g_hash_table_get_values (controller_table);
  if (val)
    {
      for (GList *l = controllers; l; l = l->next)
        {
          XrdController *controller = l->data;
          xrd_pointer_show (xrd_controller_get_pointer (controller));
          xrd_pointer_tip_show (xrd_controller_get_pointer_tip (controller));
        }
    }
  else
    {
      for (GList *l = controllers; l; l = l->next)
        {
          XrdController *controller = l->data;
          if (xrd_controller_get_hover_state (controller)->window == NULL)
            {
              xrd_pointer_hide (xrd_controller_get_pointer (controller));
              xrd_pointer_tip_hide (
                xrd_controller_get_pointer_tip (controller));
            }
        }
    }
  g_list_free (controllers);
}

static void
xrd_client_init (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);

  priv->menu = NULL;

  priv->controllers = g_hash_table_new_full (g_int64_hash, g_int64_equal,
                                             g_free, g_object_unref);

  priv->window_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);

  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "scroll-to-push-ratio",
                                  &priv->scroll_to_push_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "scroll-to-scale-ratio",
                                  &priv->scroll_to_scale_ratio);
  xrd_settings_connect_and_apply (G_CALLBACK (xrd_settings_update_double_val),
                                  "analog-threshold", &priv->analog_threshold);
  xrd_settings_connect_and_apply (G_CALLBACK (_update_show_overlay_pointer),
                                  "always-show-overlay-pointer", self);


  priv->poll_runtime_event_source_id = 0;
  priv->poll_input_source_id = 0;
  priv->keyboard_window = NULL;
  priv->keyboard_press_signal = 0;
  priv->keyboard_close_signal = 0;
  priv->pinned_only = FALSE;
  priv->selection_mode = FALSE;
  priv->ignore_input = FALSE;
  priv->wm_actions = NULL;
  priv->cursor = NULL;

  priv->context = openvr_context_get_instance ();
  priv->manager = xrd_window_manager_new ();

  priv->last_poll_timestamp = g_get_monotonic_time ();

  g_signal_connect (priv->context, "device-activate-event",
                    (GCallback) _device_activate_cb, self);
  g_signal_connect (priv->context, "device-deactivate-event",
                    (GCallback) _device_deactivate_cb, self);
}

static void
_system_quit_cb (OpenVRContext *context,
                 OpenVRQuitEvent *event,
                 XrdClient     *self)
{
  (void) event;
  (void) self;
  /* g_print("Handling VR quit event %d\n", event->reason); */
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

  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose",
                             (GCallback) _action_hand_pose_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_POSE,
                             "/actions/wm/in/hand_pose_hand_grip",
                             (GCallback) _action_hand_pose_hand_grip_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/grab_window",
                             (GCallback) _action_grab_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/reset_orientation",
                             (GCallback) _action_reset_orientation_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/menu",
                             (GCallback) _action_menu_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull_scale",
                             (GCallback) _action_push_pull_scale_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_ANALOG,
                             "/actions/wm/in/push_pull",
                             (GCallback) _action_push_pull_cb, self);
  openvr_action_set_connect (priv->wm_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/wm/in/show_keyboard",
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


gboolean
xrd_client_is_hovering (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  GList *controllers = g_hash_table_get_values (priv->controllers);
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      if (xrd_controller_get_hover_state (controller)->window != NULL)
        {
          g_list_free (controllers);
          return TRUE;
        }
    }
  g_list_free (controllers);
  return FALSE;
}

gboolean
xrd_client_is_grabbing (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  GList *controllers = g_hash_table_get_values (priv->controllers);
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      if (xrd_controller_get_grab_state (controller)->window != NULL)
        {
          g_list_free (controllers);
          return TRUE;
        }
    }
  g_list_free (controllers);
  return FALSE;
}

gboolean
xrd_client_is_grabbed (XrdClient *self,
                       XrdWindow *window)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  GList *controllers = g_hash_table_get_values (priv->controllers);
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      if (xrd_controller_get_grab_state (controller)->window == window)
        {
          g_list_free (controllers);
          return TRUE;
        }
    }
  g_list_free (controllers);
  return FALSE;
}

gboolean
xrd_client_is_hovered (XrdClient *self,
                       XrdWindow *window)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  GList *controllers = g_hash_table_get_values (priv->controllers);
  for (GList *l = controllers; l; l = l->next)
    {
      XrdController *controller = XRD_CONTROLLER (l->data);
      if (xrd_controller_get_hover_state (controller)->window == window)
        {
          g_list_free (controllers);
          return TRUE;
        }
    }
  g_list_free (controllers);
  return FALSE;
}

static XrdClient *
_replace_client (XrdClient *self)
{
  XrdClient *ret = NULL;
  gboolean to_scene = XRD_IS_OVERLAY_CLIENT (self);

  if (to_scene)
    {
      g_object_unref (self);
      ret = XRD_CLIENT (xrd_scene_client_new ());
      if (XRD_IS_SCENE_CLIENT (ret))
        xrd_scene_client_initialize (XRD_SCENE_CLIENT (ret));
    }
  else
    {
      g_object_unref (self);
      ret = XRD_CLIENT (xrd_overlay_client_new ());
    }
  return ret;
}

static void
_insert_into_new_hash_table (gpointer key,
                             gpointer value,
                             gpointer new_hash_table)
{
  g_hash_table_insert (new_hash_table, key, value);
}

static GHashTable *
_g_hash_table_clone_direct (GHashTable *old_table)
{
  GHashTable *new_table = g_hash_table_new (g_direct_hash, g_direct_equal);
  g_hash_table_foreach (old_table, _insert_into_new_hash_table, new_table);
  return new_table;
}

static GSList *
_get_new_window_data_list (XrdClient *self)
{
  GSList *data = NULL;
  GSList *windows = xrd_client_get_windows (self);
  for (GSList *l = windows; l; l = l->next)
    data = g_slist_append (data, xrd_window_get_data (l->data));
  return data;
}

/**
 * xrd_client_switch_mode:
 * @self: current #XrdClient to be destroyed.
 *
 * References to gulkan, openvr-glib and xrdesktop objects (like #XrdWindow)
 * will be invalid after calling this function.
 *
 * xrd_client_switch_mode() replaces each #XrdWindow with an appropriate new
 * one, preserving its transformation matrix, scaling, pinned status, etc.
 *
 * The caller is responsible for reconnecting callbacks to #XrdClient signals.
 * The caller is responsible to not use references to any previous #XrdWindow.
 * Pointers to #XrdWindowData will remain valid, however
 * #XrdWindowData->xrd_window will point to a new #XrdWindow.
 *
 * Returns: A new #XrdClient of the opposite mode than the passed one.
 */
struct _XrdClient *
xrd_client_switch_mode (XrdClient *self)
{
  XrdClientPrivate *priv = xrd_client_get_instance_private (self);
  gboolean show_only_pinned = priv->pinned_only;
  gboolean ignore_input = priv->ignore_input;

  /* original hash table will be destroyed on XrdClient destroy */
  GHashTable *window_mapping =
    _g_hash_table_clone_direct (priv->window_mapping);

  XrdWindowManager *manager = xrd_client_get_manager (self);

  /* this list preserves the order in which windows were added */
  GSList *window_data_list = _get_new_window_data_list (self);
  for (GSList *l = window_data_list; l; l = l->next)
    {
      XrdWindowData *window_data = l->data;
      if (window_data->texture)
         g_clear_object (&window_data->texture);
      xrd_client_remove_window (self, window_data->xrd_window);
      g_clear_object (&window_data->xrd_window);
    }

  XrdClient *ret = _replace_client (self);
  manager = xrd_client_get_manager (ret);
  priv = xrd_client_get_instance_private (ret);
  priv->window_mapping = window_mapping;

  for (GSList *l = window_data_list; l; l = l->next)
    {
      XrdWindowData *window_data = l->data;
      XrdWindow *window = xrd_client_window_new_from_data (ret, window_data);
      window_data->xrd_window = window;
      gboolean draggable = window_data->parent_window == NULL;

      xrd_client_add_window (ret, window, draggable, NULL);
    }
  g_slist_free (window_data_list);

  xrd_client_show_pinned_only (ret, show_only_pinned);

  priv->ignore_input = ignore_input;
  xrd_window_manager_set_hover_mode (manager, ignore_input ?
                                         XRD_HOVER_MODE_BUTTONS :
                                         XRD_HOVER_MODE_EVERYTHING);
  return ret;
}

