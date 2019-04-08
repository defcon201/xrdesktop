/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-follow-head-container.h"

#include <openvr-context.h>

#include "openvr-math.h"
#include <openvr-system.h>

#include "xrd-math.h"
#include "graphene-ext.h"

struct _XrdFollowHeadContainer
{
  GObject parent;
  XrdWindow *window;
  float distance;
  /* TODO: inertia */
  float speed;
};

G_DEFINE_TYPE (XrdFollowHeadContainer, xrd_follow_head_container, G_TYPE_OBJECT)

static void
xrd_follow_head_container_finalize (GObject *gobject);

static void
xrd_follow_head_container_class_init (XrdFollowHeadContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_follow_head_container_finalize;
}

static void
xrd_follow_head_container_init (XrdFollowHeadContainer *self)
{
  self->window = NULL;
  self->distance = 0;
  self->speed = 0;
}

void
xrd_follow_head_container_set_window (XrdFollowHeadContainer *self,
                                      XrdWindow *window,
                                      float distance)
{
  self->speed = 0;
  self->window = window;
  self->distance = distance;
}

XrdWindow*
xrd_follow_head_container_get_window (XrdFollowHeadContainer *self)
{
  return self->window;
}

float
xrd_follow_head_container_get_distance (XrdFollowHeadContainer *self)
{
  return self->distance;
}

float
xrd_follow_head_container_get_speed (XrdFollowHeadContainer *self)
{
  return self->speed;
}

void
xrd_follow_head_container_set_speed (XrdFollowHeadContainer *self,
                                     float speed)
{
  self->speed = speed;
}

void
_hmd_facing_pose (graphene_matrix_t *hmd_pose,
                  graphene_point3d_t *look_at_point_ws,
                  graphene_matrix_t *pose_ws)
{
  graphene_point3d_t hmd_location;
  graphene_matrix_get_translation_point3d (hmd_pose, &hmd_location);

  graphene_point3d_t look_at_from_hmd = {
    .x = look_at_point_ws->x - hmd_location.x,
    .y = look_at_point_ws->y - hmd_location.y,
    .z = look_at_point_ws->z - hmd_location.z
  };

  graphene_vec3_t look_at_direction;
  graphene_point3d_to_vec3 (&look_at_from_hmd, &look_at_direction);

  float azimuth, inclination;
  xrd_math_get_rotation_angles (&look_at_direction, &azimuth, &inclination);

  graphene_matrix_init_identity (pose_ws);
  graphene_matrix_rotate_x (pose_ws, inclination);
  graphene_matrix_rotate_y (pose_ws, - azimuth);

  xrd_math_matrix_set_translation_point (pose_ws, look_at_point_ws);
}

gboolean
xrd_follow_head_container_step (XrdFollowHeadContainer *fhc)
{
  XrdWindow *window = xrd_follow_head_container_get_window (fhc);
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
  graphene_matrix_get_translation_vec3 (&window_transform_cs, &window_vec_cs);

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

  float radius = xrd_follow_head_container_get_distance (fhc);

  /* azimuth: angle between view direction to window, "left-right" component.
   * inclination: angle between view directiont to window, "up-down" component.
   *
   * This reduces the problem in 3D space to a problem in 2D "angle space"
   * where azimuth and inclination are in [-180°,180°]x[-180°,180°].
   *
   * First the case where the window is completely out of view is handled:
   * Snapping the window towards the view direction is done by clamping
   * azimuth and inclination both towards the view direction angles (0, 0) until
   * any of the angles of the target FOV is hit.
   *
   * Then the case where the window is "too close" to being out of view is
   * handled: Angles describing a final target location for the window are
   * calculated by repeating the process towards a scaled down FOV, but it is
   * not snapped to the new position, but per frame moves proportional to the
   * remaining distance towards the target location.
   * */

  float azimuth, inclination;
  xrd_math_get_rotation_angles (&window_vec_cs, &azimuth, &inclination);

  /* Bail early when the window already is in the "center area".
   * However still update the pose to reflect movement towards/away. */
  if (azimuth > left_inner && azimuth < right_inner &&
      inclination < top_inner && inclination > bottom_inner)
  {
    //g_print ("Not moving head following window!\n");
    graphene_point3d_t new_pos_ws;
    xrd_math_sphere_to_3d_coords (azimuth, inclination, radius, &new_pos_ws);
    graphene_matrix_transform_point3d (&hmd_pose, &new_pos_ws, &new_pos_ws);

    graphene_matrix_t new_window_pose_ws;
    _hmd_facing_pose (&hmd_pose, &new_pos_ws, &new_window_pose_ws);

    xrd_window_set_transformation_matrix (window, &new_window_pose_ws);
    return TRUE;
  }

  /* Window is not visible: snap it onto the edge of the visible area. */
  if (azimuth < left_outer || azimuth > right_outer ||
      inclination > top_outer || inclination < bottom_outer)
    {

      /* delta is used to snap windows a little closer towards the view center
       * because natural head movement doesn't suddenly stop, it slows down,
       * making it snap very small distances before it leaves the snap phase
       * and enters the smooth movement phase. This would make the transition
       * from slowing snapping to fast smooth movement look like a jump. */
      float delta = 1.0;
      graphene_point_t bottom_left = {
        .x = left_outer + delta,
        .y = bottom_outer + delta
      };
      graphene_point_t top_right = {
        .x = right_outer - delta,
        .y = top_outer - delta
      };
      graphene_point_t azimuth_inclination = { .x = azimuth, .y = inclination };

      graphene_point_t intersection_azimuth_inclination;
      gboolean intersects =
          xrd_math_clamp_towards_zero_2d (&bottom_left, &top_right,
                                          &azimuth_inclination,
                                          &intersection_azimuth_inclination);

      /* doesn't happen */
      if (!intersects)
        {
          g_print ("Head Following Window should intersect, but doesn't!\n");
          return TRUE;
        }

      graphene_point3d_t new_pos_ws;
      xrd_math_sphere_to_3d_coords (intersection_azimuth_inclination.x,
                                    intersection_azimuth_inclination.y, radius,
                                    &new_pos_ws);
      graphene_matrix_transform_point3d (&hmd_pose, &new_pos_ws, &new_pos_ws);

      graphene_matrix_t new_window_pose_ws;
      _hmd_facing_pose (&hmd_pose, &new_pos_ws, &new_window_pose_ws);

      xrd_window_set_transformation_matrix (window, &new_window_pose_ws);


      graphene_vec2_t velocity;
      graphene_vec2_init (&velocity,
                          inclination - intersection_azimuth_inclination.y,
                          azimuth - intersection_azimuth_inclination.x);
      xrd_follow_head_container_set_speed (fhc,
                                           graphene_vec2_length (&velocity));

      //g_print ("Snap window to view frustum edge!\n");
      return TRUE;
    }


  /* Window is visible, but not in center area: move it towards center area*/
  graphene_point_t bottom_left = { .x = left_inner, .y = bottom_inner };
  graphene_point_t top_right = { .x = right_inner, .y = top_inner };
  graphene_point_t azimuth_inclination = { .x = azimuth, .y = inclination };

  graphene_point_t intersection_azimuth_inclination;
  gboolean intersects =
    xrd_math_clamp_towards_zero_2d (&bottom_left, &top_right,
                                    &azimuth_inclination,
                                    &intersection_azimuth_inclination);

  /* doesn't happen */
  if (!intersects)
    {
      g_print ("Head Following Window should intersect, but doesn't!\n");
      return TRUE;
    }

  float azimuth_diff = azimuth - intersection_azimuth_inclination.x;
  float inclination_diff = inclination - intersection_azimuth_inclination.y;

  /* To avoid sudden jumps in velocity:
   * Window starts with velocity 0.
   * The vertical and horizontal difference to the target angles is the
   * direction.
   * Speed_factor is the fraction of the difference angles to decrease this
   * frame, i.e. a velocity for this frame time.
   * Use this velocity, if the window already is moving that fast.
   * If not, then increase the window's speed with 1/10 of that speed.*/
  graphene_vec2_t angle_velocity;
  graphene_vec2_init (&angle_velocity, azimuth_diff, inclination_diff);
  float angle_distance = graphene_vec2_length (&angle_velocity);
  float distance_speed_factor = 0.05;
  float angle_speed = angle_distance * distance_speed_factor;

  float speed = xrd_follow_head_container_get_speed (fhc);
  if (speed < angle_speed)
    {
      speed += angle_speed / 10.;
      angle_speed = speed;
    }
  else
      speed = angle_speed;


  graphene_vec2_normalize (&angle_velocity, &angle_velocity);
  graphene_vec2_scale (&angle_velocity, angle_speed, &angle_velocity);

  graphene_vec2_t current_angles;
  graphene_vec2_init (&current_angles, azimuth, inclination);

  graphene_vec2_t next_angles;
  graphene_vec2_subtract (&current_angles, &angle_velocity, &next_angles);

  graphene_point3d_t next_point_cs;
  xrd_math_sphere_to_3d_coords (graphene_vec2_get_x (&next_angles),
                                graphene_vec2_get_y (&next_angles),
                                radius, &next_point_cs);

  graphene_point3d_t next_point_ws;
  graphene_matrix_transform_point3d (&hmd_pose, &next_point_cs, &next_point_ws);

  graphene_matrix_t new_window_pose_ws;
  _hmd_facing_pose (&hmd_pose, &next_point_ws, &new_window_pose_ws);
  xrd_window_set_transformation_matrix (window, &new_window_pose_ws);

  //g_print ("Moving head following window with %f!\n", angle_speed);
  return TRUE;
}

XrdFollowHeadContainer *
xrd_follow_head_container_new (void)
{
  return (XrdFollowHeadContainer*) g_object_new (XRD_TYPE_FOLLOW_HEAD_CONTAINER, 0);
}

static void
xrd_follow_head_container_finalize (GObject *gobject)
{
  XrdFollowHeadContainer *self = XRD_FOLLOW_HEAD_CONTAINER (gobject);
  (void) self;
}
