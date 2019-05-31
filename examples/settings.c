/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Christoph Haag <christoph.haag@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * running in development after schema change
 * 1. compile schema: glib-compile-schemas res/
 * 2. use env var: GSETTINGS_SCHEMA_DIR=./res/ build/examples/settings
 */

#include <gio/gio.h>

int main (int argc, char **argv)
{
  (void) argc;
  (void) argv;

  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  GSettingsSchema *schema =
      g_settings_schema_source_lookup (source, "org.xrdesktop", TRUE);

  gchar **keys = g_settings_schema_list_keys (schema);

  g_print ("Config keys:\n");
  for (int key_num = 0; ; key_num++)
    {
      gchar *k = keys[key_num];
      if (!k)
        break;

      GSettingsSchemaKey *schemakey = g_settings_schema_get_key (schema, k);
      GVariant *val = g_settings_schema_key_get_default_value (schemakey);

      g_print ("\t- [%s] %s: %s\n",
               g_variant_get_type_string (val),
               k,
               g_variant_print (val, TRUE));
    }

  /*
  GSettings *s = g_settings_new ("org.xrdesktop");
  float width = g_settings_get_double (s, "pointer-tip-width");
  g_print ("pointer tip width %f\n", width);
   */
}
