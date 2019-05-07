/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef XRD_GLIB_SETTINGS_H_
#define XRD_GLIB_SETTINGS_H_

#include <gio/gio.h>

GSettings *
xrd_settings_get_instance ();

void
xrd_settings_destroy_instance ();

void
xrd_settings_connect_and_apply (GCallback callback, gchar *key, gpointer data);

void
xrd_settings_update_double_val (GSettings *settings,
                                gchar *key,
                                double *val);
void
xrd_settings_update_int_val (GSettings *settings,
                             gchar *key,
                             int *val);

#endif /* XRD_GLIB_SETTINGS_H_ */
