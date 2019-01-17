/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib-unix.h>

#include "openvr-context.h"
#include "openvr-overlay.h"
#include "openvr-io.h"
#include "openvr-action-set.h"
#include "xrd-overlay-model.h"

typedef struct Example
{
  GSList *controllers;
  XrdOverlayModel *model_overlay;
  GMainLoop *loop;
  guint current_model_list_index;
  GSList *models;
  OpenVRActionSet *action_set;
} Example;

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}


static void
_pose_cb (OpenVRAction    *action,
          OpenVRPoseEvent *event,
          Example         *self)
{
  (void) action;

  graphene_point3d_t translation_point;

  graphene_point3d_init (&translation_point, .0f, .1f, -.1f);

  graphene_matrix_t transformation_matrix;
  graphene_matrix_init_translate (&transformation_matrix, &translation_point);

  graphene_matrix_scale (&transformation_matrix, 1.0f, 1.0f, 0.5f);

  graphene_matrix_t transformed;
  graphene_matrix_multiply (&transformation_matrix,
                            &event->pose,
                            &transformed);

  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self->model_overlay),
                                        &transformed);

  free (event);
}

gboolean
_update_model (Example *self)
{
  struct HmdColor_t color = {
    .r = 1.0f,
    .g = 1.0f,
    .b = 1.0f,
    .a = 1.0f
  };

  GSList* name = g_slist_nth (self->models, self->current_model_list_index);
  g_print ("Setting Model '%s' [%d/%d]\n",
           (gchar *) name->data,
           self->current_model_list_index + 1,
           g_slist_length (self->models));

  if (!xrd_overlay_model_set_model (self->model_overlay,
                               (gchar *) name->data, &color))
    return FALSE;

  return TRUE;
}

gboolean
_next_model (Example *self)
{
  if (self->current_model_list_index == g_slist_length (self->models) - 1)
    self->current_model_list_index = 0;
  else
    self->current_model_list_index++;

  if (!_update_model (self))
    return FALSE;

  return TRUE;
}

gboolean
_previous_model (Example *self)
{
  if (self->current_model_list_index == 0)
    self->current_model_list_index = g_slist_length (self->models) - 1;
  else
    self->current_model_list_index--;

  if (!_update_model (self))
    return FALSE;

  return TRUE;
}

static void
_next_cb (OpenVRAction       *action,
          OpenVRDigitalEvent *event,
          Example            *self)
{
  (void) action;
  (void) event;

  if (event->active && event->changed && event->state)
    _next_model (self);
}

static void
_previous_cb (OpenVRAction       *action,
          OpenVRDigitalEvent *event,
          Example            *self)
{
  (void) action;
  (void) event;

  if (event->active && event->changed && event->state)
    _previous_model (self);
}

gboolean
_poll_events_cb (gpointer _self)
{
  Example *self = (Example*) _self;

  if (!openvr_action_set_poll (self->action_set))
    return FALSE;

  return TRUE;
}


gboolean
_init_model_overlay (Example *self)
{
  self->model_overlay = xrd_overlay_model_new ("model", "A 3D model overlay");

  struct HmdColor_t color = {
    .r = 1.0f,
    .g = 1.0f,
    .b = 1.0f,
    .a = 1.0f
  };

  GSList* model_name = g_slist_nth (self->models,
                                    self->current_model_list_index);
  if (!xrd_overlay_model_set_model (self->model_overlay,
                               (gchar *) model_name->data, &color))
    return FALSE;

  char name_ret[k_unMaxPropertyStringSize];
  struct HmdColor_t color_ret = {};

  uint32_t id;
  if (!xrd_overlay_model_get_model (self->model_overlay, name_ret, &color_ret, &id))
    return FALSE;

  g_print ("GetOverlayRenderModel returned id %d name: %s\n", id, name_ret);

  if (!openvr_overlay_set_width_meters (OPENVR_OVERLAY (self->model_overlay),
                                        0.5f))
    return FALSE;

  if (!openvr_overlay_show (OPENVR_OVERLAY (self->model_overlay)))
    return FALSE;

  return TRUE;
}

static void
_print_model (gpointer name, gpointer unused)
{
  (void) unused;
  g_print ("Model: %s\n", (gchar*) name);
}

void
_cleanup (Example *self)
{
  g_print ("bye\n");
  g_main_loop_unref (self->loop);

  g_object_unref (self->model_overlay);
  g_slist_free_full (self->models, g_free);

  OpenVRContext *context = openvr_context_get_instance ();
  g_object_unref (context);
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
        "example_model_actions.json",
        "example_model_bindings_vive_controller.json",
        NULL))
    return -1;

  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .current_model_list_index = 0,
    .models = NULL,
    .action_set = openvr_action_set_new_from_url ("/actions/model")
  };

  openvr_action_set_connect (self.action_set, OPENVR_ACTION_DIGITAL,
                             "/actions/model/in/next",
                             (GCallback) _next_cb, &self);

  openvr_action_set_connect (self.action_set, OPENVR_ACTION_DIGITAL,
                             "/actions/model/in/previous",
                             (GCallback) _previous_cb, &self);

  openvr_action_set_connect (self.action_set, OPENVR_ACTION_POSE,
                             "/actions/model/in/hand_primary",
                             (GCallback) _pose_cb, &self);

  self.models = openvr_context_get_model_list (context);
  g_slist_foreach (self.models, _print_model, NULL);

  if (!_init_model_overlay (&self))
    return -1;

  g_timeout_add (20, _poll_events_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);

  _cleanup (&self);

  return 0;
}
