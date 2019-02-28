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

  /* TODO interpolate scaling instead of width */
  xrd_window_set_scaling_factor (window, interpolated_scaling);

  transition->interpolate += 0.03f;

  if (transition->interpolate > 1)
    {
      xrd_window_set_transformation_matrix (window, &transition->to);
      xrd_window_set_scaling_factor (window, transition->to_scaling);

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
      xrd_window_get_scaling_factor (window, &transition->from_scaling);

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

          xrd_window_get_scaling_factor (window, &transition->from_scaling);

          if (!openvr_math_matrix_equals (&transition->from, &transform))
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
  xrd_window_get_scaling_factor (window, scaling);
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
    self->following = g_slist_append (self->following, window);

  /* Register reset position */
  graphene_matrix_t *transform = graphene_matrix_alloc ();
  xrd_window_get_transformation_matrix (window, transform);
  g_hash_table_insert (self->reset_transforms, window, transform);

  float *scaling = (float*) g_malloc (sizeof (float));
  xrd_window_get_scaling_factor (window, scaling);
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

/** openvr_math_compose_projection:
 * adapted Valve OpenVR example code available at
 * https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetProjectionRaw */
void
openvr_math_compose_projection (float fLeft, float fRight, float fTop,
                                float fBottom, float zNear, float zFar,
                                graphene_matrix_t *projection)
{
  float idx = 1.0f / (fRight - fLeft);
  float idy = 1.0f / (fBottom - fTop);
  float idz = 1.0f / (zFar - zNear);
  float sx = fRight + fLeft;
  float sy = fBottom + fTop;

  HmdMatrix44_t pmProj;
  float (*p)[4] = pmProj.m;
  p[0][0] = 2 * idx;
  p[0][1] = 0;
  p[0][2] = sx * idx;
  p[0][3] = 0;

  p[1][0] = 0;
  p[1][1] = 2 * idy;
  p[1][2] = sy * idy;
  p[1][3] = 0;

  p[2][0] = 0;
  p[2][1] = 0;
  p[2][2] = -zFar * idz;
  p[2][3] = -zFar * zNear * idz;

  p[3][0] = 0;
  p[3][1] = 0;
  p[3][2] = -1.0f;
  p[3][3] = 0;

  openvr_math_matrix44_to_graphene (&pmProj, projection);
}

/** scales the left & right angles with scale_h,
 * and top & bottom angles with scale_v. */
void
openvr_math_get_scaled_frustum (graphene_frustum_t *frustum,
                                float scale_h, float scale_v)
{
  OpenVRContext *context = openvr_context_get_instance ();
  float left, right, top, bottom;
  context->system->GetProjectionRaw (EVREye_Eye_Left,
                                     &left, &right, &top, &bottom);

  /* atan gives the degree angle, scale that, and then tan again. */
  left = tan (atan (left) * scale_h);
  right = tan (atan (right) * scale_h);
  top = tan (atan (top) * scale_v);
  bottom = tan (atan (bottom) * scale_v);

  graphene_matrix_t projection_matrix;
  openvr_math_compose_projection (left, right, top, bottom, 0.1, 100,
                                  &projection_matrix);
  graphene_frustum_init_from_matrix (frustum, &projection_matrix);
}

/** Finds the nearest intersection. Ignores further intersections.
 * Operates in camera space. */
gboolean
_intersect_frustum (graphene_frustum_t *frustum,
                    graphene_ray_t *ray,
                    graphene_point3d_t *intersection)
{
  graphene_plane_t planes[6];
  graphene_frustum_get_planes (frustum, planes);

  float nearest_plane_dist = INFINITY;

  /* plane 1-4 are left/righ/bottom/top, ignore 5-6 (near/far) */
  for (int i = 0; i < 4; i++)
    {
      float distance = graphene_ray_get_distance_to_plane (ray, &planes[i]);
      if (distance != INFINITY)
        {
          //g_print ("Dist: %f\n", distance);
          if (distance < nearest_plane_dist)
            {

              /* planes extend back, we only want intersections with the part of
               * the frustum that extends in view direction. */
              graphene_point3d_t pt;
              graphene_ray_get_position_at (ray, nearest_plane_dist, &pt);
              if (pt.z > - 0.01)
                continue;

              nearest_plane_dist = distance;
            }
        }
    }

  if (nearest_plane_dist == INFINITY)
    {
      g_print ("No nearest plane\n");
      return FALSE;
    }

  /* a little bit longer so the intersection is inside the frustum */
  graphene_ray_get_position_at (ray, nearest_plane_dist + 0.0001, intersection);

  return TRUE;
}


gboolean
_clamp_point_to_frustum (graphene_frustum_t *frustum,
                         graphene_point3d_t *point,
                         graphene_point3d_t *clamped_point)
{

  gboolean in_frustum = graphene_frustum_contains_point (frustum, point);
  if (in_frustum)
      return FALSE;

  /* Find out where on the frustum we need to put the point.
   * We are in camera space, hmd is at (0, 0, 0) & looks at (0, 0, -z).
   * Imagine a sphere around the hmd that goes through the overlay.
   * This sphere intersects the view axis at (0, 0, -radius).
   *
   * A ray from the overlay location to this intersection point intersects the
   * frustum plane at the "correct" view angle to place it just on the edge of
   * the frustum.
   * It only needs to be projected back to radius dist, keeping the view angle.
   */

  float radius =
      graphene_point3d_distance (graphene_point3d_zero (), point, NULL);

  graphene_point3d_t center_view_point = { .x = 0, .y = 0, .z = -radius };

  graphene_vec3_t direction;
  graphene_vec3_init (&direction,
                      center_view_point.x - point->x,
                      center_view_point.y - point->y,
                      center_view_point.z - point->z);

  graphene_ray_t overlay_center_view_ray;
  graphene_ray_init (&overlay_center_view_ray, point, &direction);

  graphene_point3d_t frustum_intersection;
  _intersect_frustum (frustum, &overlay_center_view_ray, &frustum_intersection);

  graphene_vec3_t frustum_intersection_vec;
  graphene_vec3_init (&frustum_intersection_vec, frustum_intersection.x,
                      frustum_intersection.y, frustum_intersection.z);

  graphene_ray_t frustum_projection_ray;
  graphene_ray_init (&frustum_projection_ray, graphene_point3d_zero (),
                     &frustum_intersection_vec);

  graphene_ray_get_position_at (&frustum_projection_ray, radius,
                                clamped_point);

  return TRUE;
}


#define PI   ((float) 3.1415926535)
#define DEG_TO_RAD(x) ( (x) * 2.0 * PI / 360.0 )
#define RAD_TO_DEG(x) ( (x) * 360.0 / ( 2.0 * PI ) )

/** openvr_math_get_rotation_angles:
 * calculates 2 angles akin to the spherical coordinate system:
 * - inclination is upwards from the xz plane.
 * - azimuth is clockwise around the y axis, starting at -z. */
void
openvr_math_get_rotation_angles (graphene_vec3_t *direction,
                                 float *inclination,
                                 float *azimuth)
{
  /* y axis = 90° up. angle diff to y axis when looking up = 0°: 90°-0°=90°
   * Looking up, angle to y axis shrinks to 0° -> 90°-0°=90° inclination.
   * Looking down, angle to y axis grows to -90° -> 90°--90°=-90° incl. */
  graphene_vec3_t y_axis;
  graphene_vec3_init_from_vec3 (&y_axis, graphene_vec3_y_axis ());
  graphene_vec3_t cross;
  graphene_vec3_cross (&y_axis, direction, &cross);
  float mag = graphene_vec3_length (&cross);
  float dot = graphene_vec3_dot (&y_axis, direction);
  *inclination = 90 - RAD_TO_DEG (atan2 (mag, dot));

  /* rotation around y axis, "left-right" */
  *azimuth =
      RAD_TO_DEG (atan2 (- graphene_vec3_get_x (direction),
                         - graphene_vec3_get_z (direction)));
}


/** _nearest_frustum_edge_pose:
 * calculates a pose with the point clamped to the edge of the frustum,
 * Rotation is normal to the camera and parallel to the ground (xz plane). */
void
_nearest_frustum_edge_pose (graphene_matrix_t *camera_pose,
                            graphene_frustum_t *frustum,
                            graphene_point3d_t *point_cs,
                            graphene_matrix_t *new_pose)
{
  graphene_point3d_t clamped_cs;
  _clamp_point_to_frustum (frustum, point_cs, &clamped_cs);

  graphene_point3d_t clamped;
  graphene_matrix_transform_point3d (camera_pose, &clamped_cs, &clamped);

  graphene_vec3_t clamped_direction_cs;
  graphene_vec3_init (&clamped_direction_cs,
                      clamped_cs.x, clamped_cs.y, clamped_cs.z);

  graphene_vec3_t overlay_direction;
  graphene_matrix_transform_vec3 (camera_pose, &clamped_direction_cs,
                                  &overlay_direction);

  float inclination, azimuth;
  openvr_math_get_rotation_angles (&overlay_direction, &inclination, &azimuth);

  /* After rotating around the y axis, it would be work to find out the axis
   * for rotating upwards, so we rotate upwards from neutral position first. */
  graphene_matrix_init_identity (new_pose);
  graphene_matrix_rotate_x (new_pose, inclination);
  graphene_matrix_rotate_y (new_pose, azimuth);
  openvr_math_matrix_set_translation (new_pose, &clamped);
}

/** openvr_math_vec3_interpolate_direction:
 * interpolate between two direction vectors by angle.
 * TODO: is direct vector interpolation any worse? */
void
openvr_math_vec3_interpolate_direction (graphene_vec3_t *from,
                                        graphene_vec3_t *to,
                                        float            factor,
                                        graphene_vec3_t *res)
{
  graphene_vec3_t cross;
  graphene_vec3_cross (from, to, &cross);
  float mag = graphene_vec3_length (&cross);
  float dot = graphene_vec3_dot (from, to);
  float angle = atan2 (mag, dot);

  /* about opposite sides. TODO: proper method. */
  if (mag < 0.00001)
    {
      graphene_vec3_init (&cross, 1, 0, 0);
      mag = 1;
      angle = DEG_TO_RAD (180);
    }

  // g_print ("A: %f, %f\n", RAD_TO_DEG (angle), RAD_TO_DEG (angle * factor));

  graphene_matrix_t rot;
  graphene_matrix_init_rotate (&rot, RAD_TO_DEG (angle) * factor, &cross);

  graphene_matrix_transform_vec3 (&rot, from, res);
}

gboolean
_follow_head (XrdWindow *window)
{
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
  openvr_math_matrix_get_translation (&window_transform_cs, &window_vec_cs);
  graphene_point3d_t window_location_cs;
  graphene_point3d_init_from_vec3 (&window_location_cs, &window_vec_cs);

  /* First, if overlay is already near the frustum center, do nothing. */
  graphene_frustum_t frustum_inner;
  openvr_math_get_scaled_frustum (&frustum_inner, 0.35, 0.3);
  gboolean in_inner =
      graphene_frustum_contains_point (&frustum_inner, &window_location_cs);
  if (in_inner)
    {
      //g_print ("Nothing to do!\n");
      return TRUE;
    }

  /* If the overlay is not visible, clamp it to the frustum.
   * Scale frustum down a little because the visible FOV isn't actually as big.
   * TODO: maybe this needs to be tweaked for wide FOV HMDs? */
  graphene_frustum_t frustum;
  openvr_math_get_scaled_frustum (&frustum, 0.75, 0.6);
  gboolean window_visible =
      graphene_frustum_contains_point (&frustum, &window_location_cs);

  if (!window_visible)
    {
      //g_print ("Clamping to field of view!\n");
      graphene_matrix_t new_window_pose;
      _nearest_frustum_edge_pose (&hmd_pose, &frustum, &window_location_cs,
                                  &new_window_pose);

      xrd_window_set_transformation_matrix (window, &new_window_pose);
      return TRUE;
    }

  //g_print ("Moving!\n");

  /* target position is where the overlay will come to rest
   * (if it is not moved again) */
  graphene_point3d_t target_window_location_cs;
  _clamp_point_to_frustum (&frustum_inner, &window_location_cs,
                           &target_window_location_cs);

  graphene_vec3_t target_window_direction_cs;
  graphene_point3d_to_vec3 (&target_window_location_cs,
                            &target_window_direction_cs);

  /* this frame the overlay will be moved *on a spherical curve around the hmd*
   * in the direction of the target.
   * First, find the new position of the overlay. */
  const float factor = 0.075;
  graphene_vec3_t new_direction_cs;
  openvr_math_vec3_interpolate_direction (&window_vec_cs,
                                          &target_window_direction_cs, factor,
                                          &new_direction_cs);

  graphene_ray_t ray;
  graphene_ray_init (&ray, graphene_point3d_zero (), &new_direction_cs);

  float d = graphene_vec3_length (&window_vec_cs);

  graphene_point3d_t new_location_cs;
  graphene_ray_get_position_at (&ray, d, &new_location_cs);


  /* Now we know the new position and calculate the rotation of the overlay at
   * this position. */
  graphene_vec3_t new_direction_ws;
  graphene_matrix_transform_vec3 (&hmd_pose, &new_direction_cs,
                                  &new_direction_ws);

  float inclination, azimuth;
  openvr_math_get_rotation_angles (&new_direction_ws,
                                   &inclination, &azimuth);

  graphene_matrix_t new_pose_ws;
  graphene_matrix_init_identity (&new_pose_ws);
  graphene_matrix_rotate_x (&new_pose_ws, inclination);
  graphene_matrix_rotate_y (&new_pose_ws, azimuth);

  graphene_point3d_t new_location_ws;
  graphene_matrix_transform_point3d (&hmd_pose, &new_location_cs,
                                     &new_location_ws);
  openvr_math_matrix_set_translation (&new_pose_ws, &new_location_ws);

  xrd_window_set_transformation_matrix (window, &new_pose_ws);

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
      XrdWindow *window = (XrdWindow *) l->data;
      _follow_head (window);
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
  self->following = g_slist_remove (self->following, window);
  g_hash_table_remove (self->reset_transforms, window);

  g_object_unref (window);
}

void
_test_hover (XrdWindowManager  *self,
             graphene_matrix_t *pose,
             int                controller_index)
{
  OpenVRHoverEvent *hover_event = g_malloc (sizeof (OpenVRHoverEvent));
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
          OpenVRControllerIndexEvent *hover_start_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
          hover_start_event->index = controller_index;
          xrd_window_emit_hover_start (closest, hover_start_event);
        }

      if (closest != last_hovered_window
          && last_hovered_window != NULL)
        {
          OpenVRControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
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
          OpenVRControllerIndexEvent *hover_end_event =
              g_malloc (sizeof (OpenVRControllerIndexEvent));
          hover_end_event->index = controller_index;
          xrd_window_emit_hover_end (last_hovered_window,
                                             hover_end_event);
        }

      /* Emit no hover event every time when hovering nothing */
      OpenVRNoHoverEvent *no_hover_event =
        g_malloc (sizeof (OpenVRNoHoverEvent));
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
  xrd_window_get_scaling_factor (grab_state->window, &current_factor);

  float new_factor = current_factor + current_factor * factor * (update_rate_ms / 1000.);
  /* Don't make the overlay so small it can not be grabbed anymore */
  if (new_factor > MINIMAL_SCALE_FACTOR)
    {
      /* Grab point is relative to overlay center so we can just scale it */
      graphene_point3d_scale (&grab_state->offset_translation_point,
                              1 + factor * (update_rate_ms / 1000.),
                              &grab_state->offset_translation_point);

      xrd_window_set_scaling_factor (grab_state->window, new_factor);
    }
}

void
xrd_window_manager_check_grab (XrdWindowManager *self,
                               int               controller_index)
{
  HoverState *hover_state = &self->hover_state[controller_index];

  if (hover_state->window == NULL)
    return;

   OpenVRControllerIndexEvent *grab_event =
      g_malloc (sizeof (OpenVRControllerIndexEvent));
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

  OpenVRControllerIndexEvent *release_event =
      g_malloc (sizeof (OpenVRControllerIndexEvent));
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
