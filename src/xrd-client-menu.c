/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-client-menu.h"

#include "xrd-button.h"



typedef struct
{
  XrdWindow *button_window;

  XrdButtonType button_type;

  gboolean is_toggle_button;
  gboolean show_toggle_text;

  /* if @button_type is XRD_BUTTON_ICON, then text is a URI to the icon. */
  gchar *text;
  /* toggle text is the alternative, only set when button is a toggle button*/
  gchar *toggle_text;
} XrdButton;

struct _XrdClientMenu
{
  GObject parent;

  XrdContainerAttachment attachment;

  float button_width_meter;
  float button_height_meter;
  float button_ppm;

  int rows;
  int columns;

  GSList *buttons;

  XrdContainer *menu_container;
  XrdClient *client;
};

G_DEFINE_TYPE (XrdClientMenu, xrd_client_menu, G_TYPE_OBJECT)

static void
xrd_client_menu_finalize (GObject *gobject);

static void
xrd_client_menu_class_init (XrdClientMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xrd_client_menu_finalize;
}

static void
xrd_client_menu_init (XrdClientMenu *self)
{
  self->attachment = XRD_CONTAINER_ATTACHMENT_NONE;
  self->buttons = NULL;
  self->menu_container = NULL;
  self->button_height_meter = 0.f;
  self->button_width_meter = 0.f;
  self->button_ppm = 0.f;
}

static void
_grid_position (XrdClientMenu *self,
                float row,
                float column,
                graphene_matrix_t *relative_transform)
{
  float grid_width = self->columns * self->button_width_meter;
  float grid_height = self->rows * self->button_height_meter;

  float y_offset = grid_height / 2.f -
                   self->button_height_meter * row -
                   self->button_height_meter/ 2.f;

  float x_offset = - grid_width / 2.f +
                   self->button_width_meter * column +
                   self->button_width_meter / 2.f;

  graphene_point3d_t position;
  graphene_point3d_init (&position, x_offset, y_offset, 0);
  graphene_matrix_init_translate (relative_transform, &position);
}

void
xrd_client_menu_initialize (XrdClientMenu *self,
                            XrdClient *client,
                            XrdContainerAttachment attachment,
                            int rows,
                            int columns,
                            XrdController *controller)
{
  self->client = client;
  self->attachment = attachment;

  if (attachment == XRD_CONTAINER_ATTACHMENT_HAND)
    {
      self->button_width_meter = 0.07f;
      self->button_height_meter = 0.07f;
      self->button_ppm = 1500.0;
    }
  else
    {
      self->button_width_meter = 0.25f;
      self->button_height_meter = 0.25f;
      self->button_ppm = 450.0;
    }

  self->rows = rows;
  self->columns = columns;

  self->menu_container = xrd_container_new ();
  xrd_container_set_attachment (self->menu_container,
                                attachment,
                                controller);
  xrd_container_set_layout (self->menu_container,
                            XRD_CONTAINER_RELATIVE);

  if (attachment == XRD_CONTAINER_ATTACHMENT_HEAD)
    {
      const float distance = 2.0f;
      xrd_container_center_view (self->menu_container,
                                 distance);
      xrd_container_set_distance (self->menu_container, distance);
    }

  xrd_client_add_container (client, self->menu_container);
}

XrdWindow *
xrd_client_menu_create_button (XrdClientMenu *self,
                               XrdButtonType button_type,
                               float row,
                               float column,
                               gchar *text,
                               GCallback callback)
{
  XrdButton *button = g_malloc (sizeof (XrdButton));
  button->button_type = button_type;
  button->text = text;

  if (button_type == XRD_BUTTON_ICON)
    {
      button->button_window =
        xrd_client_button_new_from_icon (self->client,
                                         self->button_width_meter,
                                         self->button_height_meter,
                                         self->button_ppm,
                                         text);
      graphene_matrix_t relative_transform;
      _grid_position (self, row, column, &relative_transform);
      xrd_container_add_window (self->menu_container,
                                button->button_window,
                                &relative_transform);

      /* position where button is initially created doesn't matter. */
      graphene_point3d_t position = { .x =  0, .y = 0, .z = 0 };
      xrd_client_add_button (self->client, button->button_window, &position,
                             (GCallback) callback, self->client);

    }

  self->buttons = g_slist_append (self->buttons, button);
  return button->button_window;

}

static XrdButton *
_find_button (XrdClientMenu *self, XrdWindow *button_window)
{
  for (GSList *l = self->buttons; l; l = l->next)
    {
      XrdButton *xrd_button = l->data;
      if (xrd_button->button_window == button_window)
        return xrd_button;
    }
  return NULL;
}

static void
_toggle_button (XrdClient *client, XrdButton *button, gboolean show_toggle_text)
{
  if (button->show_toggle_text == show_toggle_text)
      return;

  button->show_toggle_text = show_toggle_text;

  if (button->button_type == XRD_BUTTON_ICON)
    {
      GulkanClient *gc = xrd_client_get_uploader (client);
      VkImageLayout layout = xrd_client_get_upload_layout (client);

      gchar *url =
        button->show_toggle_text ?
          button->toggle_text : button->text;
      xrd_button_set_icon (button->button_window, gc, layout, url);
    }
}

void
xrd_client_menu_set_button_toggleable (XrdClientMenu *self,
                                       XrdWindow *button_window,
                                       gchar *alt_text,
                                       gboolean show_toggle_text)
{
  XrdButton *button = _find_button (self, button_window);
  if (!button)
    {
      g_print ("Menu error: did not find button!\n");
      return;
    }

  button->show_toggle_text = FALSE;

  button->is_toggle_button = TRUE;
  button->toggle_text = alt_text;

  _toggle_button (self->client, button, show_toggle_text);
}

gboolean
xrd_client_menu_is_button_toggled (XrdClientMenu *self,
                                   XrdWindow *button_window)
{
  XrdButton *button = _find_button (self, button_window);
  if (button)
    return button->show_toggle_text;
  else
    return FALSE;
}

void
xrd_client_menu_toggle_button (XrdClientMenu *self,
                               XrdWindow *button_window,
                               gboolean show_toggle_text)
{
  XrdButton *button = _find_button (self, button_window);
  _toggle_button (self->client, button, show_toggle_text);

}

XrdClientMenu *
xrd_client_menu_new (void)
{
  return (XrdClientMenu*) g_object_new (XRD_TYPE_CLIENT_MENU, 0);
}

static void
xrd_client_menu_finalize (GObject *gobject)
{
  XrdClientMenu *self = XRD_CLIENT_MENU (gobject);

  for (GSList *l = self->buttons; l; l = l->next)
    {
      XrdButton *xrd_button = l->data;
      xrd_client_remove_window (self->client, xrd_button->button_window);
      g_clear_object (&xrd_button->button_window);
    }

  xrd_client_remove_container (self->client, self->menu_container);
  g_clear_object (&self->menu_container);

  g_slist_free (self->buttons);
}
