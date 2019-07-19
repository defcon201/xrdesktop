/*
 * xrdesktop
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */


#ifndef XRD_GLIB_CLIENT_MENU_H_
#define XRD_GLIB_CLIENT_MENU_H_

#if !defined (XRD_INSIDE) && !defined (XRD_COMPILATION)
#error "Only <xrd.h> can be included directly."
#endif


#include <glib-object.h>

#include "xrd-client.h"

typedef enum
{
  XRD_BUTTON_ICON,
  XRD_BUTTON_TEXT
} XrdButtonType;

G_BEGIN_DECLS

#define XRD_TYPE_CLIENT_MENU xrd_client_menu_get_type()
G_DECLARE_FINAL_TYPE (XrdClientMenu, xrd_client_menu, XRD, CLIENT_MENU, GObject)

XrdClientMenu *xrd_client_menu_new (void);

void
xrd_client_menu_initialize (XrdClientMenu *self,
                            XrdClient *client,
                            XrdContainerAttachment attachment,
                            int rows,
                            int columns,
                            XrdController *controller);

XrdWindow *
xrd_client_menu_create_button (XrdClientMenu *self,
                               XrdButtonType button_type,
                               float row,
                               float column,
                               gchar *text,
                               GCallback callback);

gboolean
xrd_client_menu_is_button_toggled (XrdClientMenu *self,
                                   XrdWindow *button_window);

void
xrd_client_menu_toggle_button (XrdClientMenu *self,
                               XrdWindow *button_window,
                               gboolean show_toggle_text);

void
xrd_client_menu_set_button_toggleable (XrdClientMenu *self,
                                       XrdWindow *button_window,
                                       gchar *alt_text,
                                       gboolean show_toggle_text);

G_END_DECLS

#endif /* XRD_GLIB_CLIENT_MENU_H_ */
