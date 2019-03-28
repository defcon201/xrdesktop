/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>
#include <math.h>

#include "xrd-window-manager.h"
#include "openvr-overlay.h"
#include "openvr-math.h"
#include "xrd-math.h"
#include <openvr-context.h>

typedef struct {
  XrdWindow *window;
  float distance;

  /* TODO: inertia */
  float speed;

} FollowHeadWindow;

G_DEFINE_TYPE (XrdWindowManager, xrd_window_manager, G_TYPE_OBJECT)

#define MINIMAL_SCALE_FACTOR 0.01

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

void
_free_matrix_cb (gpointer m)
{
  graphene_matrix_free ((graphene_matrix_t*) m);
}

static void
xrd_window_manager_init (XrdWindowManager *self)
{
  self->draggable_windows = NULL;
  self->managed_windows = NULL;
  self->destroy_windows = NULL;
  self->hoverable_windows = NULL;

  self->reset_transforms = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  NULL, _free_matrix_cb);
  self->reset_scalings = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL, g_free);

  for (int i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    {
      self->hover_state[i].distance = 1.0f;
      self->hover_state[i].window = NULL;
      self->grab_state[i].window = NULL;
    }
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

  g_slist_free_full (self->destroy_windows, g_object_unref);
}

gboolean
_interpolate_cb (gpointer _transition)
{
  TransformTransition *transition = (TransformTransition *) _transition;

  XrdWindow *window = transition->window;

  graphene_matrix_t interpolated;
  openvr_math_matrix_interpolate (&transition->from,
                                  &transition->to,
                                   transition->interpolate,
                                  &interpolated);
  xrd_window_set_transformation_matrix (window, &interpolated);

  float interpolated_scaling =
    transition->from_scaling * (1.0f - transition->interpolate) +
    transition->to_scaling * transition->interpolate;

  GValue g_interpolated_scaling = G_VALUE_INIT;
  g_value_init(&g_interpolated_scaling, G_TYPE_FLOAT);
  g_value_set_float (&g_interpolated_scaling, interpolated_scaling);

  /* TODO interpolate scaling instead of width */
  g_object_set_property (G_OBJECT(window), "scaling-factor",
                        &g_interpolated_scaling);

  transition->interpolate += 0.03f;

  if (transition->interpolate > 1)
    {
      xrd_window_set_transformation_matrix (window, &transition->to);

      GValue g_to_scaling = G_VALUE_INIT;
      g_value_init(&g_to_scaling, G_TYPE_FLOAT);
      g_value_set_float (&g_to_scaling, transition->to_scaling);

      g_object_set_property (G_OBJECT(window), "scaling-factor",
                            &g_to_scaling);

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

      graphene_matrix_t *transform =
        g_hash_table_lookup (self->reset_transforms, window);

      xrd_window_get_transformation_matrix (window, &transition->from);

      float *scaling = g_hash_table_lookup (self->reset_scalings, window);
      transition->to_scaling = *scaling;

      g_object_get (G_OBJECT(window), "scaling-factor", &transition->from_scaling, NULL);

      if (!xrd_math_matrix_equals (&transition->from, transform))
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
xrd_window_manager_arrange_sphere (XrdWindowManager *self)
{
  guint num_overlays = g_slist_length (self->managed_windows);
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

          XrdWindow *window =
              (XrdWindow *) g_slist_nth_data (self->managed_windows, i);

          if (window == NULL)
            {
              g_printerr ("Window %d does not exist!\n", i);
              return FALSE;
            }

          xrd_window_get_transformation_matrix (window, &transition->from);

          g_object_get (G_OBJECT(window), "scaling-factor", &transition->from_scaling, NULL);

          if (!xrd_math_matrix_equals (&transition->from, &transform))
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
  xrd_window_get_transformation_matrix (window, transform);

  float *scaling = g_hash_table_lookup (self->reset_scalings, window);
  g_object_get (G_OBJECT(window), "scaling-factor", scaling, NULL);
}

gboolean
openvr_system_get_hmd_pose (graphene_matrix_t *pose);

float
xrd_math_hmd_window_distance (XrdWindow *window)
{
  graphene_matrix_t hmd_pose;
  if (!openvr_system_get_hmd_pose (&hmd_pose))
    /* TODO: can't retry until we have a pose, will block the desktop */
    return 2.5;


  graphene_point3d_t hmd_location;
  xrd_math_matrix_get_translation_point (&hmd_pose, &hmd_location);


  graphene_matrix_t window_pose;
  xrd_window_get_transformation_matrix (window, &window_pose);
  graphene_point3d_t window_location;
  xrd_math_matrix_get_translation_point (&window_pose, &window_location);

  return graphene_point3d_distance (&hmd_location, &window_location, NULL);
}

void
xrd_window_manager_add_window (XrdWindowManager *self,
                               XrdWindow *window,
                               XrdWindowFlags flags)
{
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

  if (flags & XRD_WINDOW_FOLLOW_HEAD)
    {
      FollowHeadWindow *fhw = g_malloc (sizeof (FollowHeadWindow));

      fhw->window = window;
      fhw->distance = xrd_math_hmd_window_distance (window);
      fhw->speed = 0;

      self->following = g_slist_append (self->following, fhw);
    }

  /* Register reset position */
  graphene_matrix_t *transform = graphene_matrix_alloc ();
  xrd_window_get_transformation_matrix (window, transform);
  g_hash_table_insert (self->reset_transforms, window, transform);

  float *scaling = (float*) g_malloc (sizeof (float));
  g_object_get (G_OBJECT(window), "scaling-factor", scaling, NULL);
  g_hash_table_insert (self->reset_scalings, window, scaling);

  g_object_ref (window);
}

gboolean
openvr_system_get_hmd_pose (graphene_matrix_t *pose)
{
  OpenVRContext *context = openvr_context_get_instance ();
  VRControllerState_t state;
  if (context->system->IsTrackedDeviceConnected(k_unTrackedDeviceIndex_Hmd) &&
      context->system->GetTrackedDeviceClass (k_unTrackedDeviceIndex_Hmd) ==
          ETrackedDeviceClass_TrackedDeviceClass_HMD &&
      context->system->GetControllerState (k_unTrackedDeviceIndex_Hmd,
                                           &state, sizeof(state)))
    {
      /* k_unTrackedDeviceIndex_Hmd should be 0 => posearray[0] */
      TrackedDevicePose_t openvr_pose;
      context->system->GetDeviceToAbsoluteTrackingPose (context->origin, 0,
                                                        &openvr_pose, 1);
      openvr_math_matrix34_to_graphene (&openvr_pose.mDeviceToAbsoluteTracking,
                                        pose);

      return openvr_pose.bDeviceIsConnected &&
             openvr_pose.bPoseIsValid &&
             openvr_pose.eTrackingResult ==
                 ETrackingResult_TrackingResult_Running_OK;
    }
  return FALSE;
}

void
_hmd_facing_pose (graphene_matrix_t *hmd_pose,
                  graphene_point3d_t *look_at_point_ws,
                  graphene_matrix_t *pose_ws)
{
  graphene_point3d_t hmd_location;
  xrd_math_matrix_get_translation_point (hmd_pose, &hmd_location);

  graphene_point3d_t look_at_from_hmd = {
    .x = look_at_point_ws->x - hmd_location.x,
    .y = look_at_point_ws->y - hmd_location.y,
    .z = look_at_point_ws->z - hmd_location.z
  };

  graphene_vec3_t look_at_direction;
  graphene_point3d_to_vec3 (&look_at_from_hmd, &look_at_direction);

  float inclination, azimuth;
  xrd_math_get_rotation_angles (&look_at_direction, &inclination, &azimuth);

  graphene_matrix_init_identity (pose_ws);
  graphene_matrix_rotate_x (pose_ws, inclination);
  graphene_matrix_rotate_y (pose_ws, - azimuth);

  xrd_math_matrix_set_translation_point (pose_ws, look_at_point_ws);
}

void
_get_point_cs  (float inclination,
                float azimuth,
                float distance,
                graphene_point3d_t *point)
{

  float dist_2d = distance * cos (DEG_TO_RAD (inclination));
  graphene_point3d_init (point,
                         dist_2d * sin (DEG_TO_RAD (azimuth)),
                         distance * sin (DEG_TO_RAD (inclination)),
                         - dist_2d * cos (DEG_TO_RAD (azimuth)));
}

/* Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
 * intersect the intersection point may be stored in the floats i_x and i_y.
 * Based on an algorithm in Andre LeMothe's
 * "Tricks of the Windows Game Programming Gurus".
 * Implementation from https://stackoverflow.com/a/1968345 */
gboolean
get_line_intersection (float p0_x, float p0_y, float p1_x, float p1_y,
                       float p2_x, float p2_y, float p3_x, float p3_y,
                       float *i_x, float *i_y)
{
  float s1_x, s1_y, s2_x, s2_y;
  s1_x = p1_x - p0_x;
  s1_y = p1_y - p0_y;

  s2_x = p3_x - p2_x;
  s2_y = p3_y - p2_y;

  float s, t;
  s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) /
      (-s2_x * s1_y + s1_x * s2_y);
  t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) /
      (-s2_x * s1_y + s1_x * s2_y);

  if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
      // Collision detected
      if (i_x != NULL)
        *i_x = p0_x + (t * s1_x);
      if (i_y != NULL)
        *i_y = p0_y + (t * s1_y);
      return TRUE;
    }
  return FALSE; // No collision
}

/** _angle_space_intersect_fov:
 * In angle space a sphere is a 2D plane from [-180,180]x[-180,180] and the
 * left / right / top / bottom angles yield a rectangle.
 * To snap a window towards the edge of the FOV we don't just clamp the angle
 * to the fov angle, because that would move the window perpendicular to the
 * FOV edges which does not feel right.
 * Instead imagine a line from the origin of angle space (the center view axis)
 * towards the window's angles. Intersect this line with the FOV angles to
 * yield intersection angles that move the window towards the center view axis
 * until it becomes visible on an edge of the FOV.
 * */
gboolean
_angle_space_intersect_fov (float left, float right, float top, float bottom,
                            float inclination, float azimuth,
                            float *inclination_intersection,
                            float *azimuth_intersection)
{
  /* left */
  if (get_line_intersection (0, 0, azimuth, inclination,
                             left, bottom, left, top,
                             azimuth_intersection, inclination_intersection))
    return TRUE;

  /* right */
  if (get_line_intersection (0, 0, azimuth, inclination,
                             right, bottom, right, top,
                             azimuth_intersection, inclination_intersection))
    return TRUE;

  /* top */
  if (get_line_intersection (0, 0, azimuth, inclination,
                             left, top , right, top,
                             azimuth_intersection, inclination_intersection))
    return TRUE;

  /* bottom */
  if (get_line_intersection (0, 0, azimuth, inclination,
                             left, bottom , right, bottom,
                             azimuth_intersection, inclination_intersection))
    return TRUE;


  return FALSE;
}

gboolean
_follow_head (FollowHeadWindow *fhw)
{
  XrdWindow *window = fhw->window;
  graphene_matrix_t hmd_pose;
  openvr_system_get_hmd_pose (&hmd_pose);
  graphene_matrix_t hmd_pose_inv;
  graphene_matrix_inverse (&hmd_pose, &hmd_pose_inv);

  /* _cs means camera (hmd) space, _ws means world space. */
  graphene_matrix_t window_transform_ws;
  xrd_window_get_transformation_matrix (window, &window_transform_ws);
  graphene_matrix_t window_transform_cs;
  graphene_matrix_multiply (&window_transform_ws, &hmd_pose_inv,
                            &window_transform_cs);

  graphene_vec3_t window_vec_cs;
  xrd_math_matrix_get_translation_vec (&window_transform_cs, &window_vec_cs);

  float left, right, top, bottom;
  xrd_math_get_frustum_angles (&left, &right, &top, &bottom);

  float left_inner = left * 0.4;
  float right_inner = right * 0.4;
  float top_inner = top * 0.4;
  float bottom_inner = bottom * 0.4;

  float left_outer = left * 0.7;
  float right_outer = right * 0.7;
  float top_outer = top * 0.7;
  float bottom_outer = bottom * 0.7;

  float radius = fhw->distance;

  float inclination, azimuth;
  xrd_math_get_rotation_angles (&window_vec_cs, &inclination, &azimuth);

  /* Bail early when the window already is in the "center area".
   * Even when we don't move the window, we still recalculate the pose
   * because the hmd can move and we want to keep the window on a sphere
   * around the hmd */
    if (inclination < top_inner && inclination > bottom_inner &&
      azimuth > left_inner && azimuth < right_inner)
    {
      //g_print ("Not moving head following window!\n");
      graphene_point3d_t new_pos_ws;
      _get_point_cs (inclination, azimuth, radius, &new_pos_ws);
      graphene_matrix_transform_point3d (&hmd_pose, &new_pos_ws, &new_pos_ws);

      graphene_matrix_t new_window_pose_ws;
      _hmd_facing_pose (&hmd_pose, &new_pos_ws, &new_window_pose_ws);

      xrd_window_set_transformation_matrix (window, &new_window_pose_ws);
      return TRUE;
    }

  /* Window is not visible: snap it onto the edge of the visible area. */
  if (inclination > top_outer || inclination < bottom_outer ||
      azimuth < left_outer || azimuth > right_outer)
    {

      /* delta is used to snap windows a little closer towards the view center
       * because natural head movement doesn't suddenly stop, it slows down,
       * making it snap very small distances before it leaves the snap phase
       * and enters the smooth movement phase. This would make the transition
       * from slowing snapping to fast smooth movement look like a jump. */
      float delta = 1.0;

      float i_inclination, i_azimuth;
      gboolean intersects =
          _angle_space_intersect_fov (left_outer + delta, right_outer - delta,
                                      top_outer - delta, bottom_outer + delta,
                                      inclination, azimuth,
                                      &i_inclination, &i_azimuth);

      /* doesn't happen */
      if (!intersects)
        {
          g_print ("Head Following Window should intersect, but doesn't!\n");
          return TRUE;
        }

      /*
      g_print ("Snap from %f %f to %f %f\n",
               inclination, azimuth, i_inclination, i_azimuth);
       */

      graphene_point3d_t new_pos_ws;
      _get_point_cs (i_inclination, i_azimuth, radius, &new_pos_ws);
      graphene_matrix_transform_point3d (&hmd_pose, &new_pos_ws, &new_pos_ws);

      graphene_matrix_t new_window_pose_ws;
      _hmd_facing_pose (&hmd_pose, &new_pos_ws, &new_window_pose_ws);

      xrd_window_set_transformation_matrix (window, &new_window_pose_ws);


      graphene_vec2_t velocity;
      graphene_vec2_init (&velocity,
                          inclination - i_inclination, azimuth - i_azimuth);
      fhw->speed = graphene_vec2_length (&velocity);

      //g_print ("Snap window to view frustum edge!\n");
      return TRUE;
    }

  /* Window is visible, but not in center area: move it towards center area*/
  float i_inclination, i_azimuth;
  gboolean intersects =
      _angle_space_intersect_fov (left_inner, right_inner,
                                  top_inner, bottom_inner,
                                  inclination, azimuth,
                                  &i_inclination, &i_azimuth);

  /* doesn't happen */
  if (!intersects)
    {
      g_print ("Head Following Window should intersect, but doesn't!\n");
      return TRUE;
    }

  float vdiff = inclination - i_inclination;
  float hdiff = azimuth - i_azimuth;

  /* To avoid sudden jumps in velocity:
   * Window starts with velocity 0.
   * The vertical and horizontal difference to the target angles is the
   * direction.
   * Speed_factor is the fraction of the difference angles to decrease this
   * frame, i.e. a velocity for this frame time.
   * Use this velocity, if the window already is moving that fast.
   * If not, then increase the window's speed with 1/10 of that speed.*/
  graphene_vec2_t angle_velocity;
  graphene_vec2_init (&angle_velocity, vdiff, hdiff);
  float angle_distance = graphene_vec2_length (&angle_velocity);
  float distance_speed_factor = 0.05;
  float angle_speed = angle_distance * distance_speed_factor;
  if (fhw->speed < angle_speed)
    {
      fhw->speed += angle_speed / 10.;
      angle_speed = fhw->speed;
    }
  else
      fhw->speed = angle_speed;


  graphene_vec2_normalize (&angle_velocity, &angle_velocity);
  graphene_vec2_scale (&angle_velocity, angle_speed, &angle_velocity);

  graphene_vec2_t current_angles;
  graphene_vec2_init (&current_angles, inclination, azimuth);

  graphene_vec2_t next_angles;
  graphene_vec2_subtract (&current_angles, &angle_velocity, &next_angles);

  graphene_point3d_t next_point_cs;
  _get_point_cs (graphene_vec2_get_x (&next_angles),
                 graphene_vec2_get_y (&next_angles), radius, &next_point_cs);

  graphene_point3d_t next_point_ws;
  graphene_matrix_transform_point3d (&hmd_pose, &next_point_cs, &next_point_ws);

  graphene_matrix_t new_window_pose_ws;
  _hmd_facing_pose (&hmd_pose, &next_point_ws, &new_window_pose_ws);
  xrd_window_set_transformation_matrix (window, &new_window_pose_ws);

  //g_print ("Moving head following window with %f!\n", angle_speed);
  return TRUE;
}

void
xrd_window_manager_poll_window_events (XrdWindowManager *self)
{
  for (GSList *l = self->hoverable_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;
      xrd_window_poll_event (window);
    }

  for (GSList *l = self->following; l != NULL; l = l->next)
    {
      FollowHeadWindow *fhw = (FollowHeadWindow *) l->data;
      _follow_head (fhw);
    }
}

void
xrd_window_manager_remove_window (XrdWindowManager *self,
                                  XrdWindow *window)
{
  self->destroy_windows = g_slist_remove (self->destroy_windows, window);
  self->draggable_windows = g_slist_remove (self->draggable_windows, window);
  self->managed_windows = g_slist_remove (self->managed_windows, window);
  self->hoverable_windows = g_slist_remove (self->hoverable_windows, window);

  for (GSList *l = self->following; l != NULL; l = l->next) {
    FollowHeadWindow *fhw = (FollowHeadWindow *) l->data;
    if (fhw->window == window)
      {
        self->following = g_slist_remove (self->following, fhw);
        g_free (fhw);
      }
  }
  g_hash_table_remove (self->reset_transforms, window);

  g_object_unref (window);
}

void
_test_hover (XrdWindowManager  *self,
             graphene_matrix_t *pose,
             int                controller_index)
{
  XrdHoverEvent *hover_event = g_malloc (sizeof (XrdHoverEvent));
  hover_event->distance = FLT_MAX;

  XrdWindow *closest = NULL;

  for (GSList *l = self->hoverable_windows; l != NULL; l = l->next)
    {
      XrdWindow *window = (XrdWindow *) l->data;

      graphene_point3d_t intersection_point;
      if (xrd_window_intersects (window, pose, &intersection_point))
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

  HoverState *hover_state = &self->hover_state[controller_index];

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
          hover_start_event->index = controller_index;
          xrd_window_emit_hover_start (closest, hover_start_event);
        }

      if (closest != last_hovered_window
          && last_hovered_window != NULL)
        {
          XrdControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_end_event->index = controller_index;
          xrd_window_emit_hover_end (last_hovered_window, hover_end_event);
        }

      xrd_window_intersection_to_offset_center (closest, &hover_event->point, &hover_state->intersection_offset);

      hover_event->controller_index = controller_index;
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
          hover_state->window = NULL;
          XrdControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (XrdControllerIndexEvent));
          hover_end_event->index = controller_index;
          xrd_window_emit_hover_end (last_hovered_window,
                                             hover_end_event);
        }

      /* Emit no hover event every time when hovering nothing */
      XrdNoHoverEvent *no_hover_event = g_malloc (sizeof (XrdNoHoverEvent));
      no_hover_event->controller_index = controller_index;
      graphene_matrix_init_from_matrix (&no_hover_event->pose, pose);
      g_signal_emit (self, manager_signals[NO_HOVER_EVENT], 0,
                     no_hover_event);
    }
}

void
_drag_window (XrdWindowManager  *self,
              graphene_matrix_t *pose,
              int                controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];
  GrabState *grab_state = &self->grab_state[controller_index];

  graphene_point3d_t controller_translation_point;
  xrd_math_matrix_get_translation_point (pose, &controller_translation_point);
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

  XrdGrabEvent *event = g_malloc (sizeof (XrdGrabEvent));
  event->controller_index = controller_index;
  graphene_matrix_init_identity (&event->pose);

  /* then apply the rotation that the overlay had when it was grabbed */
  graphene_matrix_rotate_quaternion (&event->pose,
                                     &grab_state->window_rotation);

  /* reverse the rotation induced by the controller pose when it was grabbed */
  graphene_matrix_rotate_quaternion (
      &event->pose, &grab_state->window_transformed_rotation_neg);

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


  xrd_window_set_transformation_matrix (grab_state->window,
                                        &transformation_matrix);

  xrd_window_emit_grab (grab_state->window, event);
}

void
xrd_window_manager_drag_start (XrdWindowManager *self,
                               int               controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];
  GrabState *grab_state = &self->grab_state[controller_index];

  if (!_is_in_list (self->draggable_windows, hover_state->window))
    return;

  /* Copy hover to grab state */
  grab_state->window = hover_state->window;

  graphene_quaternion_t controller_rotation;
  graphene_quaternion_init_from_matrix (&controller_rotation,
                                        &hover_state->pose);

  graphene_matrix_t window_transform;
  xrd_window_get_transformation_matrix (grab_state->window, &window_transform);
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
 * xrd_window_manager_scale:
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
  g_object_get (G_OBJECT(grab_state->window), "scaling-factor", &current_factor, NULL);

  float new_factor = current_factor + current_factor * factor * (update_rate_ms / 1000.);
  /* Don't make the overlay so small it can not be grabbed anymore */
  if (new_factor > MINIMAL_SCALE_FACTOR)
    {
      /* Grab point is relative to overlay center so we can just scale it */
      graphene_point3d_scale (&grab_state->offset_translation_point,
                              1 + factor * (update_rate_ms / 1000.),
                              &grab_state->offset_translation_point);

      g_object_set (G_OBJECT(grab_state->window), "scaling-factor", new_factor, NULL);
    }
}

void
xrd_window_manager_check_grab (XrdWindowManager *self,
                               int               controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];

  if (hover_state->window == NULL)
    return;

   XrdControllerIndexEvent *grab_event =
      g_malloc (sizeof (XrdControllerIndexEvent));
  grab_event->index = controller_index;
  xrd_window_emit_grab_start (hover_state->window, grab_event);
}

void
xrd_window_manager_check_release (XrdWindowManager *self,
                                  int               controller_index)
{
  GrabState *grab_state = &self->grab_state[controller_index];

  if (grab_state->window == NULL)
    return;

  XrdControllerIndexEvent *release_event =
      g_malloc (sizeof (XrdControllerIndexEvent));
  release_event->index = controller_index;
  xrd_window_emit_release (grab_state->window, release_event);
  grab_state->window = NULL;
}

void
xrd_window_manager_update_pose (XrdWindowManager  *self,
                                graphene_matrix_t *pose,
                                int                controller_index)
{
  /* Drag test */
  if (self->grab_state[controller_index].window != NULL)
    _drag_window (self, pose, controller_index);
  else
    _test_hover (self, pose, controller_index);
}

gboolean
xrd_window_manager_is_hovering (XrdWindowManager *self)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->hover_state[i].window != NULL)
      return TRUE;
  return FALSE;
}

gboolean
xrd_window_manager_is_grabbing (XrdWindowManager *self)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->grab_state[i].window != NULL)
      return TRUE;
  return FALSE;
}

gboolean
xrd_window_manager_is_grabbed (XrdWindowManager *self,
                               XrdWindow        *window)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->grab_state[i].window == window)
      return TRUE;
  return FALSE;
}

gboolean
xrd_window_manager_is_hovered (XrdWindowManager *self,
                               XrdWindow        *window)
{
  for (uint32_t i = 0; i < OPENVR_CONTROLLER_COUNT; i++)
    if (self->hover_state[i].window == window)
      return TRUE;
  return FALSE;
}
