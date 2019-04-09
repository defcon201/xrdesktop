/*
 * Xrd GLib
 * Copyright 2019 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xrd-settings.h"

static GSettings *instance = NULL;

GSettings *
xrd_settings_get_instance ()
{
  if (!instance)
    instance = g_settings_new ("org.xrdesktop");
  return instance;
}

void
xrd_settings_destroy_instance ()
{
  if (instance)
    {
      // shouldn't happen, but better be safe
      gboolean has_unapplied;
      g_object_get (instance, "has-unapplied", &has_unapplied, NULL);
      if (has_unapplied)
        g_settings_apply (instance);
      g_object_unref (instance);
    }
  instance = NULL;
}

typedef void (*settings_callback) (GSettings *settings,
                                   gchar     *key,
                                   gpointer   user_data);

/**
 * xrd_settings_connect_and_apply:
 * @callback: A function that will be called with the given
 * @key and @data 1) immediately and 2) when the value for the given key is
 * updated.
 * @key: The settings key
 * @data: A pointer that will be passed to the update callback.
 *
 * Use this convenience function when you don't want to initially read a config
 * value from the settings, and then connect a callback to when the value
 * changes.
 *
 * Instead write only one callback that handles initially setting the value, as
 * well as any updates to this value.
 */
void
xrd_settings_connect_and_apply (GCallback callback, gchar *key, gpointer data)
{
  GSettings *settings = xrd_settings_get_instance ();

  settings_callback cb = (settings_callback) callback;
  cb (settings, key, data);

  GString *detailed_signal = g_string_new ("changed::");
  g_string_append (detailed_signal, key);

  g_signal_connect (settings, detailed_signal->str, callback, data);

  g_string_free (detailed_signal, TRUE);
}
