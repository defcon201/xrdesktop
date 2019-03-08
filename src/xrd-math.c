#include "xrd-math.h"

#include <inttypes.h>

#include <openvr-math.h>
#include <openvr-context.h>

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
                              float *inclination,
                              float *azimuth)
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

// TODO: missing in upstream
/**
 * graphene_point_scale:
 * @p: a #graphene_point_t
 * @factor: the scaling factor
 * @res: (out caller-allocates): return location for the scaled point
 *
 * Scales the coordinates of the given #graphene_point_t by
 * the given @factor.
 */
void
graphene_point_scale (const graphene_point_t *p,
                      float                   factor,
                      graphene_point_t       *res)
{
  graphene_point_init (res, p->x * factor, p->y * factor);
}
