/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_SCENE_OBJECT_H_
#define XRD_SCENE_OBJECT_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif

#include <glib-object.h>
#include <gulkan.h>
#include <graphene.h>
#include <openvr-glib.h>

G_BEGIN_DECLS

#define XRD_TYPE_SCENE_OBJECT xrd_scene_object_get_type()
G_DECLARE_DERIVABLE_TYPE (XrdSceneObject, xrd_scene_object,
                          XRD, SCENE_OBJECT, GObject)

/**
 * XrdSceneObjectClass:
 * @parent: The object class structure needs to be the first
 *   element in the widget class structure in order for the class mechanism
 *   to work correctly. This allows a XrdSceneObjectClass pointer to be cast to
 *   a GObjectClass pointer.
 */
struct _XrdSceneObjectClass
{
  GObjectClass parent;
};

XrdSceneObject *xrd_scene_object_new (void);

void
xrd_scene_object_set_scale (XrdSceneObject *self, float scale);

void
xrd_scene_object_set_position (XrdSceneObject     *self,
                               graphene_point3d_t *position);

void
xrd_scene_object_set_rotation_euler (XrdSceneObject   *self,
                                     graphene_euler_t *euler);

void
xrd_scene_object_update_mvp_matrix (XrdSceneObject    *self,
                                    EVREye             eye,
                                    graphene_matrix_t *vp);

void
xrd_scene_object_bind (XrdSceneObject    *self,
                       EVREye             eye,
                       VkCommandBuffer    cmd_buffer,
                       VkPipelineLayout   pipeline_layout);

gboolean
xrd_scene_object_initialize (XrdSceneObject        *self,
                             VkDescriptorSetLayout *layout);

void
xrd_scene_object_update_descriptors_texture (XrdSceneObject *self,
                                             VkSampler       sampler,
                                             VkImageView     image_view);

void
xrd_scene_object_update_descriptors (XrdSceneObject *self);

void
xrd_scene_object_set_transformation (XrdSceneObject    *self,
                                     graphene_matrix_t *mat);

graphene_matrix_t
xrd_scene_object_get_transformation (XrdSceneObject *self);

graphene_matrix_t
xrd_scene_object_get_transformation_no_scale (XrdSceneObject *self);

gboolean
xrd_scene_object_is_visible (XrdSceneObject *self);
void
xrd_scene_object_show (XrdSceneObject *self);

void
xrd_scene_object_hide (XrdSceneObject *self);

void
xrd_scene_object_set_transformation_direct (XrdSceneObject    *self,
                                            graphene_matrix_t *mat);

VkBuffer
xrd_scene_object_get_transformation_buffer (XrdSceneObject *self, uint32_t eye);

VkDescriptorSet
xrd_scene_object_get_descriptor_set (XrdSceneObject *self, uint32_t eye);

G_END_DECLS

#endif /* XRD_SCENE_OBJECT_H_ */
