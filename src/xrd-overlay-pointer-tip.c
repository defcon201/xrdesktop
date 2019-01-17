/*
 * OpenVR GLib
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <gdk/gdk.h>
#include "xrd-overlay-pointer-tip.h"
#include "openvr-math.h"

G_DEFINE_TYPE (XrdOverlayPointerTip, xrd_overlay_pointer_tip, OPENVR_TYPE_OVERLAY)

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject);

static void
xrd_overlay_pointer_tip_class_init (XrdOverlayPointerTipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_overlay_pointer_tip_finalize;
}

static void
xrd_overlay_pointer_tip_init (XrdOverlayPointerTip *self)
{
  (void) self;
  self->active = FALSE;
  self->texture = NULL;
  self->animation_callback_id = 0;
  self->animation_data = NULL;
}

/* the backing texture behind the intersection overlay will have
 * WIDTH*2 x HEIGHT*2 pixels to keep room for the pulse animation */
#define WIDTH 64
#define HEIGHT 64

#define ACTIVE_R 0.078
#define ACTIVE_G 0.471
#define ACTIVE_B 0.675

#define INACTIVE_R 1.0
#define INACTIVE_G 1.0
#define INACTIVE_B 1.0

#define BACKGROUND_ALPHA 0.1

typedef struct Animation
{
  XrdOverlayPointerTip *self;
  OpenVROverlayUploader *uploader;
  float progress;
} Animation;

/** draws a circle in the center of a cairo surface of dimensions WIDTHxHEIGHT.
 * scale affects the radius of the circle and should be in [0,2].
 * a_in is the alpha value at the center, a_out at the outer border. */
void
draw_gradient_circle (cairo_t *cr,
                      double r,
                      double g,
                      double b,
                      double a_in,
                      double a_out,
                      float  scale)
{
  double center_x = WIDTH;
  double center_y = HEIGHT;

  double radius = (WIDTH / 2.) * scale;

  cairo_pattern_t *pat = cairo_pattern_create_radial (center_x, center_y,
                                                      0.75 * radius,
                                                      center_x, center_y,
                                                      radius);
  cairo_pattern_add_color_stop_rgba (pat, 0, r, g, b, a_in);

  cairo_pattern_add_color_stop_rgba (pat, 1, r, g, b, a_out);
  cairo_set_source (cr, pat);
  cairo_arc (cr, center_x, center_y, radius, 0, 2 * M_PI);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);
}

/** _render_tip_pixbuf:
 * Renders the pointer tip with the desired colors.
 * If background scale is > 1, a transparent white background circle is rendered
 * behind the pointer tip. */
GdkPixbuf*
_render_tip_pixbuf (double r, double g, double b, float background_scale)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                         WIDTH * 2, HEIGHT * 2);

  cairo_t *cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  draw_gradient_circle (cr, 1.0, 1.0, 1.0, 0.1, 0.1, background_scale);

  cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);
  draw_gradient_circle (cr, r, g, b, 1.0, 0.0, 1.0);

  cairo_destroy (cr);

  /* Since we cannot set a different format for raw upload,
   * we need to use GdkPixbuf to suit OpenVRs needs */
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                                   WIDTH * 2, HEIGHT * 2);

  cairo_surface_destroy (surface);

  return pixbuf;
}

gboolean
_animate_cb (gpointer _animation)
{
  Animation *animation = (Animation *) _animation;

  GulkanClient *client = GULKAN_CLIENT (animation->uploader);
  GdkPixbuf* active_pixbuf =_render_tip_pixbuf (ACTIVE_R,
                                                ACTIVE_G,
                                                ACTIVE_B,
                                                2 - 2 * animation->progress);
  gulkan_client_upload_pixbuf (client,
                               animation->self->texture,
                               active_pixbuf);
  g_object_unref (active_pixbuf);

  openvr_overlay_uploader_submit_frame (animation->uploader,
                                       OPENVR_OVERLAY (animation->self),
                                       animation->self->texture);

  animation->progress += 0.05f;

  if (animation->progress > 1)
    {
      animation->self->animation_callback_id = 0;
      animation->self->animation_data = NULL;
      g_free (animation);
      return FALSE;
    }

  return TRUE;
}

void
xrd_overlay_pointer_tip_animate_pulse (XrdOverlayPointerTip *self,
                                       OpenVROverlayUploader *uploader)
{
  if (self->animation_callback_id != 0)
    {
      xrd_overlay_pointer_tip_set_active (self, uploader, self->active);
    }
  Animation *animation =  g_malloc (sizeof *animation);
  animation->progress = 0;
  animation->uploader = uploader;
  animation->self = self;
  self->animation_callback_id = g_timeout_add (20, _animate_cb, animation);
  self->animation_data = animation;
}

XrdOverlayPointerTip *
xrd_overlay_pointer_tip_new (int controller_index)
{
  XrdOverlayPointerTip *self =
    (XrdOverlayPointerTip*) g_object_new (XRD_TYPE_OVERLAY_POINTER_TIP, 0);

  char key[k_unVROverlayMaxKeyLength];
  snprintf (key, k_unVROverlayMaxKeyLength - 1, "intersection-%d",
            controller_index);

  /* the texture has 2x the size of the pointer, so the overlay should be 2x
   * the desired size of the default pointer too. */
  openvr_overlay_create_width (OPENVR_OVERLAY (self),
                               key, key, 0.025 * 2);

  if (!openvr_overlay_is_valid (OPENVR_OVERLAY (self)))
    {
      g_printerr ("Intersection overlay unavailable.\n");
      return NULL;
    }

  /*
   * The crosshair should always be visible, except the pointer can
   * occlude it. The pointer has max sort order, so the crosshair gets max -1
   */
  openvr_overlay_set_sort_order (OPENVR_OVERLAY (self), UINT32_MAX - 1);

  return self;
}

void
xrd_overlay_pointer_tip_init_vulkan (XrdOverlayPointerTip   *self,
                                     OpenVROverlayUploader  *uploader)
{
  GulkanClient *client = GULKAN_CLIENT (uploader);

  GdkPixbuf* pixbuf = _render_tip_pixbuf (INACTIVE_R,
                                          INACTIVE_G,
                                          INACTIVE_B,
                                          0.0);
  self->texture =
    gulkan_texture_new_from_pixbuf (client->device, pixbuf,
                                    VK_FORMAT_R8G8B8A8_UNORM);
  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  g_object_unref (pixbuf);
}

void
xrd_overlay_pointer_tip_init_raw (XrdOverlayPointerTip *self)
{
  GdkPixbuf *default_pixbuf = _render_tip_pixbuf (1.0, 1.0, 1.0, 0.0);
  openvr_overlay_set_gdk_pixbuf_raw (OPENVR_OVERLAY (self), default_pixbuf);
  g_object_unref (default_pixbuf);
}

/** xrd_overlay_pointer_tip_set_active:
 * Changes whether the active or inactive style is rendered.
 * Also cancels animations. */
void
xrd_overlay_pointer_tip_set_active (XrdOverlayPointerTip *self,
                                    OpenVROverlayUploader *uploader,
                                    gboolean active)
{
  if (self->texture == NULL)
    return;

  if (self->animation_callback_id != 0)
    {
      g_source_remove (self->animation_callback_id);
      self->animation_callback_id = 0;
      g_free (self->animation_data);
      self->animation_data = NULL;
    }

  /* Do not skip renderint to the texture even when self->active == active.
   * An animation changes the texture, so when an animation is cancelled, we
   * want to re-render the current state. */

  GulkanClient *client = GULKAN_CLIENT (uploader);

  GdkPixbuf* pixbuf = active ?
      _render_tip_pixbuf (ACTIVE_R, ACTIVE_G, ACTIVE_B, 0.0) :
      _render_tip_pixbuf (INACTIVE_R, INACTIVE_G, INACTIVE_B, 0.0);

  gulkan_client_upload_pixbuf (client, self->texture, pixbuf);
  g_object_unref (pixbuf);

  openvr_overlay_uploader_submit_frame (uploader, OPENVR_OVERLAY (self),
                                       self->texture);

  self->active = active;

}

static void
xrd_overlay_pointer_tip_finalize (GObject *gobject)
{
  XrdOverlayPointerTip *self = XRD_OVERLAY_POINTER_TIP (gobject);
  (void) self;
  if (self->texture)
    g_object_unref (self->texture);
}

/* TODO: scene app needs device poses too. Put in openvr_system? */
static gboolean
_get_hmd_pose (graphene_matrix_t *pose)
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

/* note: Set overlay transform before calling this function */
void
xrd_overlay_pointer_tip_set_constant_width (XrdOverlayPointerTip *self)
{
  graphene_matrix_t intersection_pose;
  openvr_overlay_get_transform_absolute (OPENVR_OVERLAY(self),
                                         &intersection_pose);

  graphene_vec3_t intersection_point_vec;
  openvr_math_matrix_get_translation (&intersection_pose,
                                      &intersection_point_vec);
  graphene_point3d_t intersection_point;
  graphene_point3d_init_from_vec3 (&intersection_point, &intersection_point_vec);

  /* The tip should have the same size relative to the view of the HMD.
   * Therefore we first need the HMD pose. */
  graphene_matrix_t hmd_pose;
  gboolean has_pose = _get_hmd_pose (&hmd_pose);
  if (!has_pose)
    {
      g_print ("Error: NO HMD POSE\n");
      openvr_overlay_set_width_meters (OPENVR_OVERLAY(self),
                                       DEFAULT_INTERSECTION_WIDTH);
      return;
    }

  /* To find screen space sizes we need a projection matrix. We will use only
   * the left eye, should be close enough. */
  graphene_matrix_t projection_matrix =
      openvr_system_get_projection_matrix (EVREye_Eye_Left, 0.1, 1000.);

  graphene_point3d_t intersection_screenspace;
  float w = 1.0;
  openvr_math_worldspace_to_screenspace (&intersection_point,
                                         &hmd_pose,
                                         &projection_matrix,
                                         &intersection_screenspace,
                                         &w);

  /* Transforming a point with offset of the desired screenspace distance back
   * into worldspace, reusing the same w, yields an approximate point with the
   * desired radius in worldspace. */
  graphene_point3d_t intersection_right_screenspace = {
    intersection_screenspace.x + SCREENSPACE_INTERSECTION_WIDTH / 2.,
    intersection_screenspace.y,
    intersection_screenspace.z
  };

  graphene_point3d_t intersection_right_worldspace;
  openvr_math_screenspace_to_worldspace (&intersection_right_screenspace,
                                         &hmd_pose,
                                         &projection_matrix,
                                         &intersection_right_worldspace,
                                         &w);

  graphene_vec3_t distance_vec;
  graphene_point3d_distance (&intersection_point,
                             &intersection_right_worldspace,
                             &distance_vec);
  float new_width = graphene_vec3_length (&distance_vec);

  openvr_overlay_set_width_meters (OPENVR_OVERLAY(self), new_width);
}

void
xrd_overlay_pointer_tip_update (XrdOverlayPointerTip *self,
                                graphene_matrix_t    *pose,
                                graphene_point3d_t   *intersection_point)
{
  graphene_matrix_t transform;
  graphene_matrix_init_from_matrix (&transform, pose);
  openvr_math_matrix_set_translation (&transform, intersection_point);
  openvr_overlay_set_transform_absolute (OPENVR_OVERLAY (self), &transform);

  xrd_overlay_pointer_tip_set_constant_width (self);
}
