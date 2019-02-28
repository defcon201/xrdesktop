#include "xrd-math.h"

#include <inttypes.h>

#include <openvr-math.h>

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
