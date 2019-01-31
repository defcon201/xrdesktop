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

#endif /* XRD_GLIB_SETTINGS_H_ */
