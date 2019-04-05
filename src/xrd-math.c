#include "xrd-math.h"

#include <inttypes.h>

#include <openvr-math.h>
#include <openvr-context.h>

#include "graphene-ext.h"

bool
xrd_math_matrix_equals (graphene_matrix_t *a,
                        graphene_matrix_t *b)
{
  float a_f[16];
  float b_f[16];

  graphene_matrix_to_float (a, a_f);
  graphene_matrix_to_float (b, b_f);

  for (uint32_t i = 0; i < 16; i++)
    if (a_f[i] != b_f[i])
      return FALSE;

  return TRUE;
}

float
xrd_math_point_matrix_distance (graphene_point3d_t *intersection_point,
                                graphene_matrix_t  *pose)
{
  graphene_vec3_t intersection_vec;
  graphene_point3d_to_vec3 (intersection_point, &intersection_vec);

  graphene_vec3_t pose_translation;
  openvr_math_matrix_get_translation (pose, &pose_translation);

  graphene_vec3_t distance_vec;
  graphene_vec3_subtract (&pose_translation,
                          &intersection_vec,
                          &distance_vec);

  return graphene_vec3_length (&distance_vec);
}

/* TODO: Put GetProjectionRaw into openvr-glib */
void
xrd_math_get_frustum_angles (float *left, float *right,
                             float *top, float *bottom)
{
  OpenVRContext *context = openvr_context_get_instance ();
  context->system->GetProjectionRaw (EVREye_Eye_Left,
                                     left, right, top, bottom);

  *left = RAD_TO_DEG (atan (*left));
  *right = RAD_TO_DEG (atan (*right));
  *top = - RAD_TO_DEG (atan (*top));
  *bottom = - RAD_TO_DEG (atan (*bottom));

  //g_print ("Get angles %f %f %f %f\n", *left, *right, *top, *bottom);
}

/** xrd_math_get_rotation_angles:
 * calculates 2 angles akin to the spherical coordinate system:
 * - inclination is upwards from the xz plane.
 * - azimuth is clockwise around the y axis, starting at -z. */
void
xrd_math_get_rotation_angles (graphene_vec3_t *direction,
                              float *azimuth,
                              float *inclination)
{
  // -z is forward, so we look "in the other direction"
  graphene_vec3_t anti_direction;
  graphene_vec3_scale (direction, 1, &anti_direction);

  /* y axis = 90° up. angle diff to y axis when looking up = 0°: 90°-0°=90°
   * Looking up, angle to y axis shrinks to 0° -> 90°-0°=90° inclination.
   * Looking down, angle to y axis grows to -90° -> 90°--90°=-90° incl. */
  graphene_vec3_t y_axis;
  graphene_vec3_init_from_vec3 (&y_axis, graphene_vec3_y_axis ());
  graphene_vec3_t cross;
  graphene_vec3_cross (&y_axis, &anti_direction, &cross);
  float mag = graphene_vec3_length (&cross);
  float dot = graphene_vec3_dot (&y_axis, &anti_direction);
  *inclination =  (90 - RAD_TO_DEG (atan2 (mag, dot)));

  /* rotation around y axis, "left-right".
   * Negate z axis because z = -1 is forward */
  *azimuth =
      RAD_TO_DEG (atan2 (graphene_vec3_get_x (&anti_direction),
                         - graphene_vec3_get_z (&anti_direction)));
}

void
xrd_math_matrix_set_translation_point (graphene_matrix_t  *matrix,
                                       graphene_point3d_t *point)
{
  float m[16];
  graphene_matrix_to_float (matrix, m);

  m[12] = point->x;
  m[13] = point->y;
  m[14] = point->z;
  graphene_matrix_init_from_float (matrix, m);
}

void
xrd_math_matrix_set_translation_vec (graphene_matrix_t  *matrix,
                                     graphene_vec3_t *vec)
{
  float m[16];
  graphene_matrix_to_float (matrix, m);

  m[12] = graphene_vec3_get_x (vec);
  m[13] = graphene_vec3_get_y (vec);
  m[14] = graphene_vec3_get_z (vec);
  graphene_matrix_init_from_float (matrix, m);
}

void
xrd_math_matrix_get_translation_vec (graphene_matrix_t *matrix,
                                     graphene_vec3_t   *vec)
{
  graphene_vec3_init (vec,
                      graphene_matrix_get_value (matrix, 3, 0),
                      graphene_matrix_get_value (matrix, 3, 1),
                      graphene_matrix_get_value (matrix, 3, 2));
}

void
xrd_math_matrix_get_translation_point (graphene_matrix_t  *matrix,
                                       graphene_point3d_t *point)
{
  graphene_point3d_init (point,
                         graphene_matrix_get_value (matrix, 3, 0),
                         graphene_matrix_get_value (matrix, 3, 1),
                         graphene_matrix_get_value (matrix, 3, 2));
}

gboolean
xrd_math_intersect_lines_2d (float p0_x, float p0_y, float p1_x, float p1_y,
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

gboolean
xrd_math_clamp_towards_zero_2d (float x_min, float x_max,
                                float y_min, float y_max,
                                float x, float y,
                                float *x_clamped, float *y_clamped)
{
  /* left */
  if (xrd_math_intersect_lines_2d (0, 0, x, y,
                                   x_min, y_min, x_min, y_max,
                                   x_clamped, y_clamped))
    return TRUE;

  /* right */
  if (xrd_math_intersect_lines_2d (0, 0, x, y,
                                   x_max, y_min, x_max, y_max,
                                   x_clamped, y_clamped))
    return TRUE;

  /* top */
  if (xrd_math_intersect_lines_2d (0, 0, x, y,
                                   x_min, y_max, x_max, y_max,
                                   x_clamped, y_clamped))
    return TRUE;

  /* bottom */
  if (xrd_math_intersect_lines_2d (0, 0, x, y,
                                   x_min, y_min, x_max, y_min,
                                   x_clamped, y_clamped))
    return TRUE;


  return FALSE;
}

void
xrd_math_sphere_to_3d_coords (float azimuth,
                              float inclination,
                              float distance,
                              graphene_point3d_t *point)
{

  float dist_2d = distance * cos (DEG_TO_RAD (inclination));
  graphene_point3d_init (point,
                         dist_2d * sin (DEG_TO_RAD (azimuth)),
                         distance * sin (DEG_TO_RAD (inclination)),
                         - dist_2d * cos (DEG_TO_RAD (azimuth)));
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
