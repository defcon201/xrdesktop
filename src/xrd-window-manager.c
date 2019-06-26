/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>
#include <math.h>

#include <openvr-glib.h>

#include "xrd-window-manager.h"
#include "xrd-math.h"
#include "graphene-ext.h"

#include "xrd-controller.h"

struct _XrdWindowManager
{
  GObject parent;

  GSList *draggable_windows;
  GSList *managed_windows;
  GSList *hoverable_windows;
  GSList *destroy_windows;
  GSList *containers;

  /* all windows except XRD_WINDOW_MANAGER_BUTTON */
  GSList *all_windows;

  /* XRD_WINDOW_MANAGER_BUTTON */
  GSList *buttons;

  GSList *pinned_windows;

  gboolean controls_shown;

  GHashTable *reset_transforms;
  GHashTable *reset_scalings;
};

G_DEFINE_TYPE (XrdWindowManager, xrd_window_manager, G_TYPE_OBJECT)

enum {
  NO_HOVER_EVENT,
  LAST_SIGNAL
};
static guint manager_signals[LAST_SIGNAL] = { 0 };

static void
xrd_window_manager_finalize (GObject *gobject);

static void
xrd_window_manager_class_init (XrdWindowManagerClass *klass)
{
  manager_signals[NO_HOVER_EVENT] =
    g_signal_new ("no-hover-event",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_FIRST,
                   0, NULL, NULL, NULL, G_TYPE_NONE,
                   1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_window_manager_finalize;
}

static void
_free_matrix_cb (gpointer m)
{
  graphene_matrix_free ((graphene_matrix_t*) m);
}

static void
xrd_window_manager_init (XrdWindowManager *self)
{
  self->all_windows = NULL;
  self->buttons = NULL;
  self->pinned_windows = NULL;
  self->draggable_windows = NULL;
  self->managed_windows = NULL;
  self->destroy_windows = NULL;
  self->hoverable_windows = NULL;

  self->reset_transforms = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  NULL, _free_matrix_cb);
  self->reset_scalings = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL, g_free);

  /* TODO: possible steamvr issue: When input poll rate is high and buttons are
   * immediately hidden after creation, they may not reappear on show().
   * For, show buttons when starting overlay client to avoid this issue . */
  self->controls_shown = TRUE;
}

XrdWindowManager *
xrd_window_manager_new (void)
{
  return (XrdWindowManager*) g_object_new (XRD_TYPE_WINDOW_MANAGER, 0);
}

static void
xrd_window_manager_finalize (GObject *gobject)
{
  XrdWindowManager *self = XRD_WINDOW_MANAGER (gobject);

  g_hash_table_unref (self->reset_transforms);
  g_hash_table_unref (self->reset_scalings);

  /* remove the window manager's reference to all windows */
  g_slist_free_full (self->all_windows, g_object_unref);
  g_slist_free_full (self->buttons, g_object_unref);

  g_slist_free (self->pinned_windows);
  g_slist_free (self->hoverable_windows);
  g_slist_free (self->containers);
  g_slist_free (self->draggable_windows);

  g_slist_free_full (self->destroy_windows, g_object_unref);
}

static gboolean
_interpolate_cb (gpointer _transition)
{
  TransformTransition *transition = (TransformTransition *) _transition;

  XrdWindow *window = transition->window;

  float interpolation_curve =
    - (float)pow ((double)transition->interpolate - 1.0, 4) + 1;

  graphene_matrix_t interpolated;
  graphene_matrix_interpolate_simple (&transition->from,
                                      &transition->to,
                                       interpolation_curve,
                                      &interpolated);
  xrd_window_set_transformation (window, &interpolated);

  float interpolated_scaling =
    transition->from_scaling * (1.0f - interpolation_curve) +
    transition->to_scaling * interpolation_curve;

  g_object_set (G_OBJECT(window), "scale", (double) interpolated_scaling, NULL);

  gint64 now = g_get_monotonic_time ();
  float ms_since_last = (now - transition->last_timestamp) / 1000.f;
  transition->last_timestamp = now;

  /* in seconds */
  const float transition_duration = 0.75;

  transition->interpolate += ms_since_last / 1000.f / transition_duration;

  if (transition->interpolate > 1)
    {
      xrd_window_set_transformation (window, &transition->to);

      g_object_set (G_OBJECT(window), "scale",
                    (double) transition->to_scaling, NULL);

      g_object_unref (transition->window);
      g_free (transition);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_is_in_list (GSList *list,
             XrdWindow *window)
{
  GSList *l;
  for (l = list; l != NULL; l = l->next)
    {
      if (l->data == window)
        return TRUE;
    }
  return FALSE;
}

void
xrd_window_manager_arrange_reset (XrdWindowManager *self)
{
  GSList *l;
  for (l = self->managed_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;

      TransformTransition *transition = g_malloc (sizeof *transition);
      transition->last_timestamp = g_get_monotonic_time ();

      graphene_matrix_t *transform =
        g_hash_table_lookup (self->reset_transforms, window);

      xrd_window_get_transformation_no_scale (window, &transition->from);

      float *scaling = g_hash_table_lookup (self->reset_scalings, window);
      transition->to_scaling = *scaling;

      g_object_get (G_OBJECT(window), "scale", &transition->from_scaling, NULL);

      if (!graphene_matrix_equals (&transition->from, transform))
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

static float
_ffabs (float v)
{
  return (float) fabs ((double) v);
}

static float
_azimuth_from_pose (graphene_matrix_t *mat)
{
  graphene_matrix_t rotation_matrix;
  graphene_matrix_get_rotation_matrix (mat, &rotation_matrix);

  graphene_vec3_t start;
  graphene_vec3_init (&start, 0, 0,- 1);
  graphene_vec3_t direction;
  graphene_matrix_transform_vec3 (&rotation_matrix, &start, &direction);

  return atan2f (graphene_vec3_get_x (&direction),
                -graphene_vec3_get_z (&direction));
}

gboolean
xrd_window_manager_arrange_sphere (XrdWindowManager *self)
{
  guint num_overlays = g_slist_length (self->managed_windows);

  double root_num_overlays = sqrt((double) num_overlays);

  uint32_t grid_height = (uint32_t) root_num_overlays;
  uint32_t grid_width = (uint32_t) ((float) num_overlays / (float) grid_height);

  while (grid_width * grid_height < num_overlays)
    grid_width++;

  graphene_matrix_t hmd_pose;
  openvr_system_get_hmd_pose (&hmd_pose);
  graphene_vec3_t hmd_vec;
  graphene_matrix_get_translation_vec3 (&hmd_pose, &hmd_vec);

  graphene_vec3_t hmd_vec_neg;
  graphene_vec3_negate (&hmd_vec, &hmd_vec_neg);

  float theta_fov = (float) M_PI / 2.5f;
  float theta_center = (float) M_PI / 2.0f;
  float theta_start = theta_center + theta_fov / 2.0f;
  float theta_end = theta_center - theta_fov / 2.0f;
  float theta_range = _ffabs(theta_end - theta_start);
  float theta_step = theta_range / (float) (grid_height - 1);

  float phi_fov = (float) M_PI / 2.5f;
  float phi_center = -(float) M_PI / 2.0f + _azimuth_from_pose (&hmd_pose);
  float phi_start = phi_center - phi_fov / 2.0f;
  float phi_end = phi_center + phi_fov / 2.0f;
  float phi_range = _ffabs(phi_end - phi_start);
  float phi_step = phi_range / (float)(grid_width - 1);

  float radius = 5.0f;

  guint i = 0;
  for (float theta = theta_start; theta > theta_end - 0.01f; theta -= theta_step)
    {
      for (float phi = phi_start; phi < phi_end + 0.01f; phi += phi_step)
        {
          TransformTransition *transition = g_malloc (sizeof *transition);
          transition->last_timestamp = g_get_monotonic_time ();

          float const x = sinf (theta) * cosf (phi);
          float const y = cosf (theta);
          float const z = sinf (phi) * sinf (theta);

          graphene_matrix_t transform;

          graphene_vec3_t position;
          graphene_vec3_init (&position,
                              x * radius,
                              y * radius,
                              z * radius);

          graphene_vec3_add (&position, &hmd_vec, &position);

          graphene_vec3_negate (&position, &position);

          graphene_matrix_init_look_at (&transform,
                                        &position,
                                        &hmd_vec_neg,
                                        graphene_vec3_y_axis ());

          XrdWindow *window =
              (XrdWindow *) g_slist_nth_data (self->managed_windows, i);

          if (window == NULL)
            {
              g_printerr ("Window %d does not exist!\n", i);
              return FALSE;
            }

          xrd_window_get_transformation_no_scale (window, &transition->from);

          g_object_get (G_OBJECT(window), "scale", &transition->from_scaling, NULL);

          if (!graphene_matrix_equals (&transition->from, &transform))
            {
              transition->interpolate = 0;
              transition->window = window;
              g_object_ref (window);

              graphene_matrix_init_from_matrix (&transition->to, &transform);

              float *scaling = g_hash_table_lookup (self->reset_scalings, window);
              transition->to_scaling = *scaling;

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
xrd_window_manager_save_reset_transform (XrdWindowManager *self,
                                         XrdWindow *window)
{
  graphene_matrix_t *transform =
    g_hash_table_lookup (self->reset_transforms, window);
  xrd_window_get_transformation_no_scale (window, transform);

  float *scaling = g_hash_table_lookup (self->reset_scalings, window);
  g_object_get (G_OBJECT(window), "scale", scaling, NULL);
}

void
xrd_window_manager_add_container (XrdWindowManager *self,
                                  XrdContainer *container)
{
  self->containers = g_slist_append (self->containers, container);
}

void
xrd_window_manager_remove_container (XrdWindowManager *self,
                                     XrdContainer *container)
{
  self->containers = g_slist_remove (self->containers, container);
}

void
xrd_window_manager_add_window (XrdWindowManager *self,
                               XrdWindow *window,
                               XrdWindowFlags flags)
{
  /* any window must be either in all_windows or buttons */
  if (flags & XRD_WINDOW_MANAGER_BUTTON)
    {
      self->buttons = g_slist_append (self->buttons, window);
      if (!self->controls_shown)
        xrd_window_hide (window);
    }
  else
    {
      self->all_windows = g_slist_append (self->all_windows, window);
    }

  /* Freed with manager */
  if (flags & XRD_WINDOW_DESTROY_WITH_PARENT)
    self->destroy_windows = g_slist_append (self->destroy_windows, window);

  /* Movable overlays (user can move them) */
  if (flags & XRD_WINDOW_DRAGGABLE)
    self->draggable_windows = g_slist_append (self->draggable_windows, window);

  /* Managed overlays (window manager can move them) */
  if (flags & XRD_WINDOW_MANAGED)
    self->managed_windows = g_slist_append (self->managed_windows, window);

  /* All windows that can be hovered, includes button windows */
  if (flags & XRD_WINDOW_HOVERABLE)
    self->hoverable_windows = g_slist_append (self->hoverable_windows, window);

  /* Register reset position */
  graphene_matrix_t *transform = graphene_matrix_alloc ();
  xrd_window_get_transformation_no_scale (window, transform);
  g_hash_table_insert (self->reset_transforms, window, transform);

  float *scaling = (float*) g_malloc (sizeof (float));
  g_object_get (G_OBJECT(window), "scale", scaling, NULL);
  g_hash_table_insert (self->reset_scalings, window, scaling);

  /* keep the window referenced as long as the window manages this window */
  g_object_ref (window);
}

void
xrd_window_manager_poll_window_events (XrdWindowManager *self)
{
  for (GSList *l = self->hoverable_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;
      xrd_window_poll_event (window);
    }

  for (GSList *l = self->containers; l != NULL; l = l->next)
    {
      XrdContainer *wc = (XrdContainer *) l->data;
      xrd_container_step (wc);
    }
}

void
xrd_window_manager_remove_window (XrdWindowManager *self,
                                  XrdWindow *window)
{
  self->all_windows = g_slist_remove (self->all_windows, window);
  self->buttons = g_slist_remove (self->buttons, window);
  self->destroy_windows = g_slist_remove (self->destroy_windows, window);
  self->draggable_windows = g_slist_remove (self->draggable_windows, window);
  self->managed_windows = g_slist_remove (self->managed_windows, window);
  self->hoverable_windows = g_slist_remove (self->hoverable_windows, window);

  g_hash_table_remove (self->reset_transforms, window);

  for (GSList *l = self->containers; l != NULL; l = l->next)
    {
      XrdContainer *wc = (XrdContainer *) l->data;
      xrd_container_remove_window (wc, window);
    }

  /* remove the window manager's reference to the window */
  g_object_unref (window);
}

static void
_test_hover (XrdWindowManager  *self,
             graphene_matrix_t *pose,
             XrdController     *controller)
{
  XrdHoverEvent *hover_event = g_malloc (sizeof (XrdHoverEvent));
  hover_event->distance = FLT_MAX;

  XrdWindow *closest = NULL;

  XrdPointer *pointer = xrd_controller_get_pointer (controller);

  for (GSList *l = self->hoverable_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;

      if (!xrd_window_is_visible (window))
        continue;

      graphene_point3d_t intersection_point;
      if (xrd_window_intersects (window, pointer, pose, &intersection_point))
        {
          float distance =
            xrd_math_point_matrix_distance (&intersection_point, pose);
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

  HoverState *hover_state = xrd_controller_get_hover_state (controller);

  xrd_pointer_set_selected_window (pointer, closest);

  if (closest != NULL)
    {
      /* The recipient of the hover_end event should already see that this
       * overlay is not hovered anymore, so we need to set the hover state
       * before sending the event */
      XrdWindow *last_hovered_window = hover_state->window;
      hover_state->distance = hover_event->distance;
      hover_state->window = closest;
      graphene_matrix_init_from_matrix (&hover_state->pose, pose);

      /* We now hover over an overlay */
      if (closest != last_hovered_window)
        {
          XrdControllerIndexEvent *hover_start_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_start_event->controller_handle =
            xrd_controller_get_handle (controller);
          xrd_window_emit_hover_start (closest, hover_start_event);
        }

      if (closest != last_hovered_window
          && last_hovered_window != NULL)
        {
          XrdControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_end_event->controller_handle =
            xrd_controller_get_handle (controller);
          xrd_window_emit_hover_end (last_hovered_window, hover_end_event);
        }

      xrd_window_get_intersection_2d (
        closest, &hover_event->point, &hover_state->intersection_2d);

      hover_event->controller_handle = xrd_controller_get_handle (controller);
      xrd_window_emit_hover (closest, hover_event);
    }
  else
    {
      /* No intersection was found, nothing is hovered */
      g_free (hover_event);

      /* Emit hover end event only if we had hovered something earlier */
      if (hover_state->window != NULL)
        {
          XrdWindow *last_hovered_window = hover_state->window;
          xrd_controller_reset_hover_state (controller);
          XrdControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_end_event->controller_handle =
            xrd_controller_get_handle (controller);
          xrd_window_emit_hover_end (last_hovered_window, hover_end_event);
        }

      /* Emit no hover event every time when hovering nothing */
      XrdNoHoverEvent *no_hover_event = g_malloc (sizeof (XrdNoHoverEvent));
      no_hover_event->controller_handle =
        xrd_controller_get_handle (controller);
      graphene_matrix_init_from_matrix (&no_hover_event->pose, pose);
      g_signal_emit (self, manager_signals[NO_HOVER_EVENT], 0, no_hover_event);
    }
}

static void
_drag_window (XrdWindowManager  *self,
              graphene_matrix_t *pose,
              XrdController     *controller)
{
  (void) self;
  HoverState *hover_state = xrd_controller_get_hover_state (controller);
  GrabState *grab_state = xrd_controller_get_grab_state (controller);

  graphene_point3d_t controller_translation_point;
  graphene_matrix_get_translation_point3d (pose, &controller_translation_point);
  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation, pose);

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f, 0.f, -hover_state->distance);

  /* Build a new transform for pointer tip in event->pose.
   * Pointer tip is at intersection, in the plane of the window,
   * so we can reuse the tip rotation for the window rotation. */
  XrdGrabEvent *event = g_malloc (sizeof (XrdGrabEvent));
  event->controller_handle = xrd_controller_get_handle (controller);
  graphene_matrix_init_identity (&event->pose);

  /* restore original rotation of the tip */
  graphene_matrix_rotate_quaternion (&event->pose,
                                     &grab_state->window_rotation);

  /* Later the current controller rotation is applied to the overlay, so to
   * keep the later controller rotations relative to the initial controller
   * rotation, rotate the window in the opposite direction of the initial
   * controller rotation.
   * This will initially result in the same window rotation so the window does
   * not change its rotation when being grabbed, and changing the controllers
   * position later will rotate the window with the "diff" of the controller
   * rotation to the initial controller rotation. */
  graphene_matrix_rotate_quaternion (
      &event->pose, &grab_state->inverse_controller_rotation);

  /* then translate the overlay to the controller ray distance */
  graphene_matrix_translate (&event->pose, &distance_translation_point);

  /* Rotate the translated overlay to where the controller is pointing. */
  graphene_matrix_rotate_quaternion (&event->pose,
                                     &controller_rotation);

  /* Calculation was done for controller in (0,0,0), just move it with
   * controller's offset to real (0,0,0) */
  graphene_matrix_translate (&event->pose, &controller_translation_point);



  graphene_matrix_t transformation_matrix;
  graphene_matrix_init_identity (&transformation_matrix);

  /* translate such that the grab point is pivot point. */
  graphene_matrix_translate (&transformation_matrix,
                             &grab_state->grab_offset);

  /* window has the same rotation as the tip we calculated in event->pose */
  graphene_matrix_multiply (&transformation_matrix,
                            &event->pose,
                            &transformation_matrix);

  xrd_window_set_transformation (grab_state->window,
                                 &transformation_matrix);

  xrd_window_emit_grab (grab_state->window, event);

  XrdPointer *pointer = xrd_controller_get_pointer (controller);
  xrd_pointer_set_selected_window (pointer, grab_state->window);
}

void
xrd_window_manager_drag_start (XrdWindowManager *self,
                               XrdController    *controller)
{
  HoverState *hover_state = xrd_controller_get_hover_state (controller);
  GrabState *grab_state = xrd_controller_get_grab_state (controller);

  if (!_is_in_list (self->draggable_windows, hover_state->window))
    return;

  /* Copy hover to grab state */
  grab_state->window = hover_state->window;

  graphene_quaternion_t controller_rotation;
  graphene_matrix_get_rotation_quaternion (&hover_state->pose,
                                           &controller_rotation);

  graphene_matrix_t window_transform;
  xrd_window_get_transformation_no_scale (grab_state->window, &window_transform);

  graphene_matrix_get_rotation_quaternion (&window_transform,
                                           &grab_state->window_rotation);

  graphene_point3d_t distance_translation_point;
  graphene_point3d_init (&distance_translation_point,
                         0.f, 0.f, -hover_state->distance);

  graphene_point3d_t negative_distance_translation_point;
  graphene_point3d_init (&negative_distance_translation_point,
                         0.f, 0.f, +hover_state->distance);

  graphene_point3d_init (
      &grab_state->grab_offset,
      -hover_state->intersection_2d.x,
      -hover_state->intersection_2d.y,
      0.f);

  graphene_quaternion_invert (
      &controller_rotation,
      &grab_state->inverse_controller_rotation);
}

/* checks if a float is in the range specified when the property was created */
static gboolean
_valid_float_prop (GObject *object, const gchar *prop, float value)
{
  GParamSpec *spec =
    g_object_class_find_property (G_OBJECT_GET_CLASS (object), prop);
  GValue gvalue = G_VALUE_INIT;
  g_value_init (&gvalue, G_TYPE_FLOAT);
  g_value_set_float (&gvalue, value);
  gboolean oor = g_param_value_validate (spec, &gvalue);
  return !oor;
}

/**
 * xrd_window_manager_scale:
 * @self: The #XrdWindowManager
 * @grab_state: The #GrabState to scale
 * @factor: Scale factor
 * @update_rate_ms: The update rate in ms
 *
 * While dragging a window, scale the window *factor* times per second
 */
void
xrd_window_manager_scale (XrdWindowManager *self,
                          GrabState        *grab_state,
                          float             factor,
                          float             update_rate_ms)
{
  if (grab_state->window == NULL)
    return;
  (void) self;

  float current_factor;
  g_object_get (G_OBJECT(grab_state->window), "scale", &current_factor, NULL);

  float new_factor = current_factor +
                     current_factor * factor * (update_rate_ms / 1000.f);

  if (!_valid_float_prop (G_OBJECT (grab_state->window), "scale", new_factor))
      return;

  /* Grab point is relative to overlay center so we can just scale it */
  graphene_point3d_scale (&grab_state->grab_offset,
                          1 + factor * (update_rate_ms / 1000.f),
                          &grab_state->grab_offset);

  g_object_set (G_OBJECT(grab_state->window),
                "scale", (double) new_factor, NULL);
}

void
xrd_window_manager_check_grab (XrdWindowManager *self,
                               XrdController    *controller)
{
  (void) self;
  HoverState *hover_state = xrd_controller_get_hover_state (controller);

  if (hover_state->window == NULL)
    return;

   XrdControllerIndexEvent *grab_event =
      g_malloc (sizeof (XrdControllerIndexEvent));
  grab_event->controller_handle = xrd_controller_get_handle (controller);
  xrd_window_emit_grab_start (hover_state->window, grab_event);
}

void
xrd_window_manager_check_release (XrdWindowManager *self,
                                  XrdController    *controller)
{
  (void) self;
  GrabState *grab_state = xrd_controller_get_grab_state (controller);

  if (grab_state->window == NULL)
    return;

  XrdControllerIndexEvent *release_event =
      g_malloc (sizeof (XrdControllerIndexEvent));
  release_event->controller_handle = xrd_controller_get_handle (controller);
  xrd_window_emit_release (grab_state->window, release_event);
  xrd_controller_reset_grab_state (controller);
}

void
xrd_window_manager_update_pose (XrdWindowManager  *self,
                                graphene_matrix_t *pose,
                                XrdController     *controller)
{
  /* Drag test */
  if (xrd_controller_get_grab_state (controller)->window != NULL)
    _drag_window (self, pose, controller);
  else
    _test_hover (self, pose, controller);
}

void
xrd_window_manager_set_pin (XrdWindowManager *self,
                            XrdWindow *win,
                            gboolean pin)
{
  if (pin)
    {
      if (g_slist_find (self->pinned_windows, win) == NULL)
        self->pinned_windows = g_slist_append (self->pinned_windows, win);
    }
  else
      self->pinned_windows = g_slist_remove (self->pinned_windows, win);
}

gboolean
xrd_window_manager_is_pinned (XrdWindowManager *self,
                              XrdWindow *win)
{
  return g_slist_find (self->pinned_windows, win) != NULL;
}

GSList *
xrd_window_manager_get_windows (XrdWindowManager *self)
{
  return self->all_windows;
}

GSList *
xrd_window_manager_get_buttons (XrdWindowManager *self)
{
  return self->buttons;
}

void
xrd_window_manager_show_pinned_only (XrdWindowManager *self,
                                     gboolean pinned_only)
{
  for (GSList *l = self->all_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;
      gboolean to_show = TRUE;
      if (pinned_only)
        to_show = g_slist_find (self->pinned_windows, window) != NULL;

      if (to_show)
        xrd_window_show (window);
      else
        xrd_window_hide (window);
    }
}

void
xrd_window_manager_save_state (XrdWindowManager *self,
                               XrdWindowState *state,
                               int window_count)
{
  GSList *windows = xrd_window_manager_get_windows (self);

  for (int i = 0; i < window_count; i++)
    {
      state[i].child_index = -1;
      XrdWindow *window = g_slist_nth_data (windows, (guint)i);

      state[i].pinned = xrd_window_manager_is_pinned (self, window);

      graphene_matrix_t *reset_transform =
        g_hash_table_lookup (self->reset_transforms, window);
      graphene_matrix_init_from_matrix (&state[i].reset_transform,
                                        reset_transform);

      XrdWindowData *data = xrd_window_get_data (window);

      /* Window state is saved in the order windows were added to the manager.
       * So the reference to a child window is just an index in this array. */
      if (data->child_window)
        {
          state[i].child_index = g_slist_index (windows, data->child_window);
          graphene_point_init_from_point (&state[i].child_offset_center,
                                          &data->child_offset_center);
        }

      /* Window with parent window is child and is not draggable */
      state[i].is_draggable = data->parent_window == NULL;

      g_object_get (window,
                    "native", &state[i].native,
                    "title", &state[i].title,
                    "scale", &state[i].scale,
                    "initial-width-meters", &state[i].initial_width,
                    "initial-height-meters", &state[i].initial_height,
                    "texture-width", &state[i].texture_width,
                    "texture-height", &state[i].texture_height,
                    NULL);

      state[i].current_width = xrd_window_get_current_width_meters (window);
      state[i].current_height = xrd_window_get_current_height_meters (window);
      xrd_window_get_transformation_no_scale (window, &state[i].transform);
    }
}
