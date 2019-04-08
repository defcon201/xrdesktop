#include "xrd-math.h"

#include <inttypes.h>

#include <openvr-math.h>
#include <openvr-system.h>
#include "graphene-ext.h"

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
xrd_math_intersect_lines_2d (graphene_point_t *p0, graphene_point_t *p1,
                             graphene_point_t *p2, graphene_point_t *p3,
                             graphene_point_t *intersection)
{
  graphene_point_t s1 = {
    .x = p1->x - p0->x,
    .y = p1->y - p0->y
  };

  graphene_point_t s2 = {
    .x = p3->x - p2->x,
    .y = p3->y - p2->y
  };

  float s, t;
  s = (-s1.y * (p0->x - p2->x) + s1.x * (p0->y - p2->y)) /
      (-s2.x * s1.y + s1.x * s2.y);
  t = ( s2.x * (p0->y - p2->y) - s2.y * (p0->x - p2->x)) /
      (-s2.x * s1.y + s1.x * s2.y);

  if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
      // Collision detected
      intersection->x = p0->x + (t * s1.x);
      intersection->y = p0->y + (t * s1.y);
      return TRUE;
    }
  return FALSE; // No collision
}

gboolean
xrd_math_clamp_towards_zero_2d (graphene_point_t *min,
                                graphene_point_t *max,
                                graphene_point_t *point,
                                graphene_point_t *clamped)
{
  graphene_point_t zero = { .x = 0, .y = 0};

  graphene_point_t bottom_left = { min->x, min->y };
  graphene_point_t top_left = { min->x, max->y };
  graphene_point_t top_right = { max->x, max->y };
  graphene_point_t bottom_right = { max->x, min->y };

  /* left */
  if (xrd_math_intersect_lines_2d (&zero, point,
                                   &bottom_left, &top_left,
                                   clamped))
    return TRUE;

  /* right */
  if (xrd_math_intersect_lines_2d (&zero, point,
                                   &bottom_right, &top_right,
                                   clamped))
    return TRUE;

  /* top */
  if (xrd_math_intersect_lines_2d (&zero, point,
                                   &top_left, &top_right,
                                   clamped))
    return TRUE;

  /* bottom */
  if (xrd_math_intersect_lines_2d (&zero, point,
                                   &bottom_left, &bottom_right,
                                   clamped))
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
