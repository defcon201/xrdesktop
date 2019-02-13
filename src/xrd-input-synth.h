/*
 * Xrd GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_INPUT_SYNTH_H_
#define XRD_GLIB_INPUT_SYNTH_H_

#include <glib-object.h>

#include <openvr-action-set.h>

#include "xrd-overlay-window.h"
#include "xrd-overlay-desktop-cursor.h"

G_BEGIN_DECLS

#define XRD_TYPE_INPUT_SYNTH xrd_input_synth_get_type()
G_DECLARE_FINAL_TYPE (XrdInputSynth, xrd_input_synth, XRD, INPUT_SYNTH, GObject)

typedef struct XrdClickEvent {
  XrdOverlayWindow *window;
  graphene_point_t *position;
  int               button;
  gboolean          state;
  int               controller_index;
} XrdClickEvent;

typedef struct XrdMoveCursorEvent {
  XrdOverlayWindow *window;
  graphene_point_t *position;
} XrdMoveCursorEvent;

typedef struct XrdInputSynthController
{
  XrdInputSynth *self;
  int            index;
} XrdInputSynthController;

struct _XrdInputSynth
{
  GObject parent;

  XrdInputSynthController left;
  XrdInputSynthController right;
  
  /* hover_position is relative to hover_window */
  graphene_point_t hover_position;

  uint32_t button_press_state;
  graphene_vec3_t scroll_accumulator;

  double scroll_threshold;

  int synthing_controller_index;

  OpenVRActionSet *synth_actions;
};

XrdInputSynth *
xrd_input_synth_new (void);

gboolean
xrd_input_synth_poll_events (XrdInputSynth *self);

void
xrd_input_synth_reset_scroll (XrdInputSynth *self);

void
xrd_input_synth_reset_press_state (XrdInputSynth *self);

void
xrd_input_synth_move_cursor (XrdInputSynth    *self,
                             XrdOverlayWindow *window,
                             graphene_point3d_t *intersection);

int
xrd_input_synth_synthing_controller (XrdInputSynth *self);

void
xrd_input_synth_hand_off_to_controller (XrdInputSynth *self,
                                        int controller_index);


G_END_DECLS

#endif /* XRD_GLIB_INPUT_SYNTH_H_ */
