/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>
#include <math.h>

#include "xrd-overlay-window-manager.h"
#include "openvr-overlay.h"
#include "openvr-math.h"

G_DEFINE_TYPE (XrdOverlayWindowManager, xrd_overlay_window_manager, G_TYPE_OBJECT)

#define MINIMAL_SCALE_WIDTH 0.1

enum {
  NO_HOVER_EVENT,
  LAST_SIGNAL
};
static guint overlay_manager_signals[LAST_SIGNAL] = { 0 };

static void
xrd_overlay_window_manager_finalize (GObject *gobject);

static void
xrd_overlay_window_manager_class_init (XrdOverlayWindowManagerClass *klass)
{
  overlay_manager_signals[NO_HOVER_EVENT] =
    g_signal_new ("no-hover-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_window_manager_finalize;
}

void
_free_matrix_cb (gpointer m)
{
  graphene_matrix_free ((graphene_matrix_t*) m);
}

static void
xrd_overlay_window_manager_init (XrdOverlayWindowManager *self)
{
  self->reset_transforms = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  NULL, _free_matrix_cb);
  self->reset_widths = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                              NULL, g_free);

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      self->hover_state[i].distance = 1.0f;
      self->hover_state[i].window = NULL;
      self->grab_state[i].window = NULL;
    }
}

XrdOverlayWindowManager *
xrd_overlay_window_manager_new (void)
{
  return (XrdOverlayWindowManager*) g_object_new (XRD_TYPE_OVERLAY_WINDOW_MANAGER, 0);
}

static void
xrd_overlay_window_manager_finalize (GObject *gobject)
{
  XrdOverlayWindowManager *self = XRD_OVERLAY_WINDOW_MANAGER (gobject);

  g_hash_table_unref (self->reset_transforms);
  g_hash_table_unref (self->reset_widths);

  g_slist_free_full (self->destroy_windows, g_object_unref);
}

gboolean
_interpolate_cb (gpointer _transition)
{
  TransformTransition *transition = (TransformTransition *) _transition;

  XrdOverlayWindow *window = transition->window;

  graphene_matrix_t interpolated;
  openvr_math_matrix_interpolate (&transition->from,
                                  &transition->to,
                                   transition->interpolate,
                                  &interpolated);
  xrd_overlay_window_set_transformation_matrix (window, &interpolated);

  float interpolated_width =
    transition->from_width * (1.0f - transition->interpolate) +
    transition->to_width * transition->interpolate;
  xrd_overlay_window_set_xr_width (window, interpolated_width);

  transition->interpolate += 0.03f;

  if (transition->interpolate > 1)
    {
      xrd_overlay_window_set_transformation_matrix (window, &transition->to);
      xrd_overlay_window_set_xr_width (window, transition->to_width);

      g_object_unref (transition->window);
      g_free (transition);
      return FALSE;
    }

  return TRUE;
}

void
xrd_overlay_window_manager_arrange_reset (XrdOverlayWindowManager *self)
{
  GSList *l;
  for (l = self->grab_windows; l != NULL; l = l->next)
    {
      XrdOverlayWindow *window = (XrdOverlayWindow *) l->data;

      TransformTransition *transition = g_malloc (sizeof *transition);

      graphene_matrix_t *transform =
        g_hash_table_lookup (self->reset_transforms, window);

      xrd_overlay_window_get_transformation_matrix (window, &transition->from);

      float *width = g_hash_table_lookup (self->reset_widths, window);
      transition->to_width = *width;
      xrd_overlay_window_get_xr_width (window, &transition->from_width);

      if (!openvr_math_matrix_equals (&transition->from, transform))
        {
          transition->interpolate = 0;
          transition->window = window;
          g_object_ref (window);

          graphene_matrix_init_from_matrix (&transition->to, transform);

          g_timeout_add (20, _interpolate_cb, transition);
        }
      else
        {
          g_free (transition);
        }
    }
}

gboolean
xrd_overlay_window_manager_arrange_sphere (XrdOverlayWindowManager *self)
{
  guint num_overlays = g_slist_length (self->grab_windows);
  uint32_t grid_height = (uint32_t) sqrt((float) num_overlays);
  uint32_t grid_width = (uint32_t) ((float) num_overlays / (float) grid_height);

  while (grid_width * grid_height < num_overlays)
    grid_width++;

  float theta_start = M_PI / 2.0f;
  float theta_end = M_PI - M_PI / 8.0f;
  float theta_range = theta_end - theta_start;
  float theta_step = theta_range / grid_width;

  float phi_start = 0;
  float phi_end = M_PI;
  float phi_range = phi_end - phi_start;
  float phi_step = phi_range / grid_height;

  guint i = 0;

  for (float theta = theta_start; theta < theta_end; theta += theta_step)
    {
      /* TODO: don't need hack 0.01 to check phi range */
      for (float phi = phi_start; phi < phi_end - 0.01; phi += phi_step)
        {
          TransformTransition *transition = g_malloc (sizeof *transition);

          float radius = 3.0f;

          float const x = sin (theta) * cos (phi);
          float const y = cos (theta);
          float const z = sin (phi) * sin (theta);

          graphene_matrix_t transform;

          graphene_vec3_t position;
          graphene_vec3_init (&position,
                              x * radius,
                              y * radius,
                              z * radius);

          graphene_matrix_init_look_at (&transform,
                                        &position,
                                        graphene_vec3_zero (),
                                        graphene_vec3_y_axis ());

          XrdOverlayWindow *window =
              (XrdOverlayWindow *) g_slist_nth_data (self->grab_windows, i);
          OpenVROverlay *overlay = window->overlay;

          if (overlay == NULL)
            {
              g_printerr ("Overlay %d does not exist!\n", i);
              return FALSE;
            }

          xrd_overlay_window_get_transformation_matrix (window, &transition->from);

          xrd_overlay_window_get_xr_width (window, &transition->from_width);

          if (!openvr_math_matrix_equals (&transition->from, &transform))
            {
              transition->interpolate = 0;
              transition->window = window;
              g_object_ref (window);

              graphene_matrix_init_from_matrix (&transition->to, &transform);

              float *width = g_hash_table_lookup (self->reset_widths, window);
              transition->to_width = *width;

              g_timeout_add (20, _interpolate_cb, transition);
            }
          else
            {
              g_free (transition);
            }

          i++;
          if (i > num_overlays)
            return TRUE;
        }
    }

  return TRUE;
}

void
xrd_overlay_window_manager_save_reset_transform (XrdOverlayWindowManager *self,
                                                 XrdOverlayWindow *window)
{
  graphene_matrix_t *transform =
    g_hash_table_lookup (self->reset_transforms, window);
  xrd_overlay_window_get_transformation_matrix (window, transform);

  float *width = g_hash_table_lookup (self->reset_widths, window);
  xrd_overlay_window_get_xr_width (window, width);
}

void
xrd_overlay_window_manager_add_window (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow *window,
                                       XrdOverlayWindowFlags flags)
{
  /* Freed with manager */
  if (flags & XRD_OVERLAY_WINDOW_DESTROY_WITH_PARENT)
    self->destroy_windows = g_slist_append (self->destroy_windows, window);

  /* Movable overlays */
  if (flags & XRD_OVERLAY_WINDOW_GRAB)
    self->grab_windows = g_slist_append (self->grab_windows, window);

  /* All overlays that can be hovered, includes button overlays */
  if (flags & XRD_OVERLAY_WINDOW_HOVER)
    self->hover_windows = g_slist_append (self->hover_windows, window);


  /* Register reset position */
  graphene_matrix_t *transform = graphene_matrix_alloc ();
  xrd_overlay_window_get_transformation_matrix (window, transform);
  g_hash_table_insert (self->reset_transforms, window, transform);

  float *width = (float*) g_malloc (sizeof (float));
  xrd_overlay_window_get_xr_width (window, width);
  g_hash_table_insert (self->reset_widths, window, width);

  g_object_ref (window);
}

void
xrd_overlay_window_manager_poll_overlay_events (XrdOverlayWindowManager *self)
{
  for (GSList *l = self->hover_windows; l != NULL; l = l->next)
    {
      XrdOverlayWindow *window = (XrdOverlayWindow *) l->data;
      xrd_overlay_window_poll_event (window);
    }
}

void
xrd_overlay_window_manager_remove_window (XrdOverlayWindowManager *self,
                                          XrdOverlayWindow *window)
{
  self->destroy_windows = g_slist_remove (self->destroy_windows, window);
  self->grab_windows = g_slist_remove (self->grab_windows, window);
  self->hover_windows = g_slist_remove (self->hover_windows, window);
  g_hash_table_remove (self->reset_transforms, window);

  g_object_unref (window);
}

void
_test_hover (XrdOverlayWindowManager *self,
             graphene_matrix_t *pose,
             int                controller_index)
{
  OpenVRHoverEvent *hover_event = g_malloc (sizeof (OpenVRHoverEvent));
  hover_event->distance = FLT_MAX;

  XrdOverlayWindow *closest = NULL;

  for (GSList *l = self->hover_windows; l != NULL; l = l->next)
    {
      XrdOverlayWindow *window = (XrdOverlayWindow *) l->data;

      graphene_point3d_t intersection_point;
      if (xrd_overlay_window_intersects (window, pose, &intersection_point))
        {
          float distance =
            openvr_math_point_matrix_distance (&intersection_point, pose);
          if (distance < hover_event->distance)
            {
              closest = window;
              hover_event->distance = distance;
              graphene_matrix_init_from_matrix (&hover_event->pose, pose);
              graphene_point3d_init_from_point (&hover_event->point,
                                                &intersection_point);
            }
        }
    }

  HoverState *hover_state = &self->hover_state[controller_index];

  if (closest != NULL)
    {
      /* The recipient of the hover_end event should already see that this
       * overlay is not hovered anymore, so we need to set the hover state
       * before sending the event */
      XrdOverlayWindow *last_hovered_window = hover_state->window;
      hover_state->distance = hover_event->distance;
      hover_state->window = closest;
      graphene_matrix_init_from_matrix (&hover_state->pose, pose);

      /* We now hover over an overlay */
      if (closest != last_hovered_window)
        {
          OpenVRControllerIndexEvent *hover_start_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
          hover_start_event->index = controller_index;
          xrd_overlay_window_emit_hover_start (closest, hover_start_event);
        }

      if (closest != last_hovered_window
          && last_hovered_window != NULL)
        {
          OpenVRControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
          hover_end_event->index = controller_index;
          xrd_overlay_window_emit_hover_end (last_hovered_window, hover_end_event);
        }

      xrd_overlay_window_intersection_to_offset_center (closest, &hover_event->point, &hover_state->intersection_offset);

      hover_event->controller_index = controller_index;
      xrd_overlay_window_emit_hover (closest, hover_event);
    }
  else
    {
      /* No intersection was found, nothing is hovered */
      g_free (hover_event);

      /* Emit hover end event only if we had hovered something earlier */
      if (hover_state->window != NULL)
        {
          XrdOverlayWindow *last_hovered_window = hover_state->window;
          hover_state->window = NULL;
          OpenVRControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
          hover_end_event->index = controller_index;
          xrd_overlay_window_emit_hover_end (last_hovered_window,
                                             hover_end_event);
        }

      /* Emit no hover event every time when hovering nothing */
      OpenVRNoHoverEvent *no_hover_event =
        g_malloc (sizeof (OpenVRNoHoverEvent));
      no_hover_event->controller_index = controller_index;
      graphene_matrix_init_from_matrix (&no_hover_event->pose, pose);
      g_signal_emit (self, overlay_manager_signals[NO_HOVER_EVENT], 0,
                     no_hover_event);
    }
}

void
_drag_overlay (XrdOverlayWindowManager *self,
               graphene_matrix_t *pose,
               int                controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];
  GrabState *grab_state = &self->grab_state[controller_index];

  graphene_vec3_t controller_translation;
  openvr_math_matrix_get_translation (pose, &controller_translation);
  graphene_point3d_t controller_translation_point;
  graphene_point3d_init_from_vec3 (&controller_translation_point,
                                   &controller_translation);
  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation, pose);

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f, 0.f, -hover_state->distance);

  graphene_matrix_t transformation_matrix;
  graphene_matrix_init_identity (&transformation_matrix);

  /* first translate the overlay so that the grab point is the origin */
  graphene_matrix_translate (&transformation_matrix,
                             &grab_state->offset_translation_point);

  OpenVRGrabEvent *event = g_malloc (sizeof (OpenVRGrabEvent));
  event->controller_index = controller_index;
  graphene_matrix_init_identity (&event->pose);

  /* then apply the rotation that the overlay had when it was grabbed */
  graphene_matrix_rotate_quaternion (&event->pose,
                                     &grab_state->window_rotation);

  /* reverse the rotation induced by the controller pose when it was grabbed */
  graphene_matrix_rotate_quaternion (
      &event->pose,
      &grab_state->window_transformed_rotation_neg);

  /* then translate the overlay to the controller ray distance */
  graphene_matrix_translate (&event->pose, &distance_translation_point);

  /*
   * rotate the translated overlay. Because the original controller rotation has
   * been subtracted, this will only add the diff to the original rotation
   */
  graphene_matrix_rotate_quaternion (&event->pose,
                                     &controller_rotation);

  /* and finally move the whole thing so the controller is the origin */
  graphene_matrix_translate (&event->pose, &controller_translation_point);

  /* Apply pointer tip transform to overlay */
  graphene_matrix_multiply (&transformation_matrix,
                            &event->pose,
                            &transformation_matrix);


  xrd_overlay_window_set_transformation_matrix (grab_state->window,
                                                &transformation_matrix);

  xrd_overlay_window_emit_grab (grab_state->window, event);
}

void
xrd_overlay_window_manager_drag_start (XrdOverlayWindowManager *self,
                                       int                     controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];
  GrabState *grab_state = &self->grab_state[controller_index];

  /* Copy hover to grab state */
  grab_state->window = hover_state->window;

  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation,
                                        &hover_state->pose);

  graphene_matrix_t window_transform;
  xrd_overlay_window_get_transformation_matrix (grab_state->window,
                                                &window_transform);
  graphene_quaternion_init_from_matrix (
      &grab_state->window_rotation, &window_transform);

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f, 0.f, -hover_state->distance);

  graphene_point3d_t negative_distance_translation_point;
  graphene_point3d_init (&negative_distance_translation_point,
                         0.f, 0.f, +hover_state->distance);

  graphene_point3d_init (
      &grab_state->offset_translation_point,
      -hover_state->intersection_offset.x,
      -hover_state->intersection_offset.y,
      0.f);

  /* Calculate the inverse of the overlay rotatation that is induced by the
   * controller dragging the overlay in an arc to its current location when it
   * is grabbed. Multiplying this inverse rotation to the rotation of the
   * overlay will subtract the initial rotation induced by the controller pose
   * when the overlay was grabbed.
   */
  graphene_matrix_t target_transformation_matrix;
  graphene_matrix_init_identity (&target_transformation_matrix);
  graphene_matrix_translate (&target_transformation_matrix,
                             &distance_translation_point);
  graphene_matrix_rotate_quaternion (&target_transformation_matrix,
                                     &controller_rotation);
  graphene_matrix_translate (&target_transformation_matrix,
                             &negative_distance_translation_point);
  graphene_quaternion_t transformed_rotation;
  graphene_quaternion_init_from_matrix (&transformed_rotation,
                                        &target_transformation_matrix);
  graphene_quaternion_invert (
      &transformed_rotation,
      &grab_state->window_transformed_rotation_neg);
}

/**
 * openvr_overlay_scale:
 *
 * While dragging an overlay, scale the overlay @factor times per second
 */

void
xrd_overlay_window_manager_scale (XrdOverlayWindowManager *self,
                                  GrabState *grab_state,
                                  float factor,
                                  float update_rate_ms)
{
  if (grab_state->window == NULL)
    return;
  (void) self;
  float width;

  xrd_overlay_window_get_xr_width (grab_state->window, &width);
  float new_width = width + width * factor * (update_rate_ms / 1000.);
  /* Don't make the overlay so small it can not be grabbed anymore */
  if (new_width > MINIMAL_SCALE_WIDTH)
    {
      /* Grab point is relative to overlay center so we can just scale it */
      graphene_point3d_scale (&grab_state->offset_translation_point,
                              1 + factor * (update_rate_ms / 1000.),
                              &grab_state->offset_translation_point);

      xrd_overlay_window_set_xr_width (grab_state->window, new_width);
    }
}

void
xrd_overlay_window_manager_check_grab (XrdOverlayWindowManager *self,
                                       int                     controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];

  if (hover_state->window != NULL)
    {
      OpenVRControllerIndexEvent *grab_event =
          g_malloc (sizeof (OpenVRControllerIndexEvent));
      grab_event->index = controller_index;
      xrd_overlay_window_emit_grab_start (hover_state->window, grab_event);
    }
}

void
xrd_overlay_window_manager_check_release (XrdOverlayWindowManager *self,
                                          int controller_index)
{
  GrabState *grab_state = &self->grab_state[controller_index];

  if (grab_state->window != NULL)
    {
      OpenVRControllerIndexEvent *release_event =
          g_malloc (sizeof (OpenVRControllerIndexEvent));
      release_event->index = controller_index;
      xrd_overlay_window_emit_release (grab_state->window, release_event);
    }
  grab_state->window = NULL;
}

void
xrd_overlay_window_manager_update_pose (XrdOverlayWindowManager *self,
                                        graphene_matrix_t       *pose,
                                        int controller_index)
{
  /* Drag test */
  if (self->grab_state[controller_index].window != NULL)
    _drag_overlay (self, pose, controller_index);
  else
    _test_hover (self, pose, controller_index);
}

gboolean
xrd_overlay_window_manager_is_hovering (XrdOverlayWindowManager *self)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->hover_state[i].window != NULL)
      return TRUE;
  return FALSE;
}

gboolean
xrd_overlay_window_manager_is_grabbing (XrdOverlayWindowManager *self)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->grab_state[i].window != NULL)
      return TRUE;
  return FALSE;
}

gboolean
xrd_overlay_window_manager_is_grabbed (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow *window)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->grab_state[i].window == window)
      return TRUE;
  return FALSE;
}

gboolean
xrd_overlay_window_manager_is_hovered (XrdOverlayWindowManager *self,
                                       XrdOverlayWindow *window)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->hover_state[i].window == window)
      return TRUE;
  return FALSE;
}
