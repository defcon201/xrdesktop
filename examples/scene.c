#include <glib.h>
#include <glib-unix.h>

#include "xrd-scene-client.h"

typedef struct Example
{
  XrdSceneClient *client;
  GMainLoop *loop;
} Example;

void
_cleanup (Example *self)
{
  g_print ("bye\n");
  g_object_unref (self->client);
}

gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

gboolean
_iterate_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  xrd_scene_client_render (self->client);
  return true;
}

int
main ()
{
  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .client = xrd_scene_client_new ()
  };

  if (!xrd_scene_client_initialize (self.client))
    {
      _cleanup (&self);
      return 1;
    }

  g_timeout_add (1, _iterate_cb, &self);

  g_unix_signal_add (SIGINT, _sigint_cb, &self);

  /* start glib main loop */
  g_main_loop_run (self.loop);
  g_main_loop_unref (self.loop);

  _cleanup (&self);

  return 0;
}
