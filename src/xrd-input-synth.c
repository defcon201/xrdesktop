/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-input-synth.h"

#include <gdk/gdk.h>

#include "xrd-settings.h"

G_DEFINE_TYPE (XrdInputSynth, xrd_input_synth, G_TYPE_OBJECT)

enum {
  CLICK_EVENT,
  MOVE_CURSOR_EVENT,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
xrd_input_synth_finalize (GObject *gobject);

static void
xrd_input_synth_class_init (XrdInputSynthClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_input_synth_finalize;

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

XrdInputSynth *
xrd_input_synth_new (void)
{
  return (XrdInputSynth*) g_object_new (XRD_TYPE_INPUT_SYNTH, 0);
}

static void
xrd_input_synth_finalize (GObject *gobject)
{
  XrdInputSynth *self = XRD_INPUT_SYNTH (gobject);
  g_object_unref (self->synth_actions);

  G_OBJECT_CLASS (xrd_input_synth_parent_class)->finalize (gobject);
}

void
_emit_click (XrdInputSynth    *self,
             graphene_point_t *position,
             int               button,
             gboolean          state)
{
  XrdClickEvent *click_event = g_malloc (sizeof (XrdClickEvent));
  click_event->position = position;
  click_event->button = button;
  click_event->state = state;
  click_event->controller_index = self->synthing_controller_index;
  g_signal_emit (self, signals[CLICK_EVENT], 0, click_event);
}

void
xrd_input_synth_reset_press_state (XrdInputSynth *self)
{
  if (self && self->button_press_state)
    {
      /*g_print ("End hover, button mask %d...\n", self->button_press_state); */
      for (int button = 1; button <= 8; button++)
        {
          gboolean pressed = self->button_press_state & 1 << button;
          if (pressed)
            {
              g_print ("Released button %d\n", button);
              _emit_click (self, &self->hover_position, button, FALSE);
            }
        }
      self->button_press_state = 0;
    }
}

static void
_action_left_click_cb (OpenVRAction             *action,
                       OpenVRDigitalEvent       *event,
                       XrdInputSynthController  *controller)
{
  (void) action;
  XrdInputSynth *self = controller->self;

  /* when left clicking with a controller that is *not* used to do input
   * synth, make this controller do input synth */
  if (event->state && self->synthing_controller_index != controller->index)
    {
      g_free (event);
      xrd_input_synth_hand_off_to_controller (self, controller->index);
      return;
    }

  if (self->synthing_controller_index != controller->index)
    {
      g_free (event);
      return;
    }

  if (event->changed)
    {
      _emit_click (self, &self->hover_position, 1, event->state);

      if (event->state)
        self->button_press_state |= 1 << 1;
      else
        self->button_press_state &= ~(1 << 1);
    }
  g_free (event);
}

static void
_action_right_click_cb (OpenVRAction            *action,
                        OpenVRDigitalEvent      *event,
                        XrdInputSynthController *controller)
{
  (void) action;
  XrdInputSynth *self = controller->self;

  if (self->synthing_controller_index != controller->index)
    {
      g_free (event);
      return;
    }

  if (event->changed)
    {
      _emit_click (self, &self->hover_position, 3, event->state);
      if (event->state)
        self->button_press_state |= 1 << 3;
      else
        self->button_press_state &= ~(1 << 3);
    }
  g_free (event);
}

static void
_do_scroll (XrdInputSynth *self, int steps_x, int steps_y)
{
  for (int i = 0; i < abs(steps_y); i++)
    {
      int btn;
      if (steps_y > 0)
        btn = 4;
      else
        btn = 5;
      _emit_click (self, &self->hover_position, btn, TRUE);
      _emit_click (self, &self->hover_position, btn, FALSE);
    }

  for (int i = 0; i < abs(steps_x); i++)
    {
      int btn;
      if (steps_x < 0)
        btn = 6;
      else
        btn = 7;
      _emit_click (self, &self->hover_position, btn, TRUE);
      _emit_click (self, &self->hover_position, btn, FALSE);
    }
}

/*
 * When the touchpad is touched, start adding up movement.
 * If movement is over threshold, create a scroll event and reset
 * scroll_accumulator.
 */
static void
_action_scroll_cb (OpenVRAction            *action,
                   OpenVRAnalogEvent       *event,
                   XrdInputSynthController *controller)
{
  (void) action;
  XrdInputSynth *self = controller->self;

  if (self->synthing_controller_index != controller->index)
    {
      g_free (event);
      return;
    }
  
  /* When z is not zero we get bogus data. We ignore this completely */
  if (graphene_vec3_get_z (&event->state) != 0.0)
    {
      g_free (event);
      return;
    }

  static graphene_vec3_t last_touch_pos;
  gboolean initial_touch = graphene_vec3_get_x (&last_touch_pos) == 0.0 &&
                           graphene_vec3_get_y (&last_touch_pos) == 0.0;
  graphene_vec3_init_from_vec3 (&last_touch_pos, &event->state);

  /* No touch, no need to waste processing power */
  if (graphene_vec3_get_x (&event->state) == 0.0 &&
      graphene_vec3_get_y (&event->state) == 0.0)
    {
      g_free(event);
      return;
    }

  /* when starting to touch the touchpad, we get a delta from (0,0) to where
   * the touchpad is touched. Ignore this bogus delta. */
  if (initial_touch)
    {
      g_free (event);
      return;
    }

  graphene_vec3_add (&self->scroll_accumulator, &event->delta,
                     &self->scroll_accumulator);

  float x_acc = graphene_vec3_get_x (&self->scroll_accumulator);
  float y_acc = graphene_vec3_get_y (&self->scroll_accumulator);

  /*
   * Scroll as many times as the threshold has been exceeded.
   * e.g. user scrolled 0.32 with threshold of 0.1 -> scroll 3 times.
   */
  int steps_x = x_acc / self->scroll_threshold;
  int steps_y = y_acc / self->scroll_threshold;

  /*
   * We need to keep the rest in the accumulator to not lose part of the
   * user's movement e.g. 0.32: -> 0.2 and -0.32 -> -0.2
   */
  float rest_x = x_acc - (float)steps_x * self->scroll_threshold;
  float rest_y = y_acc - (float)steps_y * self->scroll_threshold;
  graphene_vec3_init (&self->scroll_accumulator, rest_x, rest_y, 0);

  _do_scroll (self, steps_x, steps_y);

  g_free (event);
}

void
xrd_input_synth_move_cursor (XrdInputSynth    *self,
                             XrdOverlayWindow *window,
                             graphene_point_t *position)
{
  
  XrdMoveCursorEvent *event = g_malloc (sizeof (XrdClickEvent));
  event->window = window;
  event->position = position;
  g_signal_emit (self, signals[MOVE_CURSOR_EVENT], 0, event);

  graphene_point_init_from_point (&self->hover_position, position);
}

static void
_update_scroll_threshold (GSettings *settings, gchar *key, gpointer _self)
{
  XrdInputSynth *self = _self;
  self->scroll_threshold = g_settings_get_double (settings, key);
}

static void
xrd_input_synth_init (XrdInputSynth *self)
{
  self->button_press_state = 0;
  graphene_point_init (&self->hover_position, 0, 0);

  self->synth_actions = openvr_action_set_new_from_url ("/actions/mouse_synth");

  self->left.self = self;
  self->left.index = 0;

  self->right.self = self;
  self->right.index = 1;

  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_left",
                             (GCallback) _action_left_click_cb, &self->left);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/right_click_left",
                             (GCallback) _action_right_click_cb, &self->left);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_ANALOG,
                             "/actions/mouse_synth/in/scroll_left",
                             (GCallback) _action_scroll_cb, &self->left);

  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/left_click_right",
                             (GCallback) _action_left_click_cb, &self->right);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_DIGITAL,
                             "/actions/mouse_synth/in/right_click_right",
                             (GCallback) _action_right_click_cb, &self->right);
  openvr_action_set_connect (self->synth_actions, OPENVR_ACTION_ANALOG,
                             "/actions/mouse_synth/in/scroll_right",
                             (GCallback) _action_scroll_cb, &self->right);

  xrd_settings_connect_and_apply (G_CALLBACK (_update_scroll_threshold),
                                  "scroll-threshold", self);

  self->synthing_controller_index = 1;
}

int
xrd_input_synth_synthing_controller (XrdInputSynth *self)
{
  return self->synthing_controller_index;
}

void
xrd_input_synth_hand_off_to_controller (XrdInputSynth *self,
                                        int controller_index)
{
  xrd_input_synth_reset_scroll (self);
  xrd_input_synth_reset_press_state (self);
  self->synthing_controller_index = controller_index;
}

gboolean
xrd_input_synth_poll_events (XrdInputSynth *self)
{
  if (!openvr_action_set_poll (self->synth_actions))
    return FALSE;
  return TRUE;
}

void
xrd_input_synth_reset_scroll (XrdInputSynth *self)
{
  graphene_vec3_init (&self->scroll_accumulator, 0, 0, 0);
}

