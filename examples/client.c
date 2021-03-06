/*
 * xrdesktop
 * Copyright 2018 Collabora Ltd.
 * Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <glib-unix.h>

#include "xrd.h"

#define GRID_WIDTH 4
#define GRID_HEIGHT 4

typedef struct Example
{
  GMainLoop *loop;
  XrdClient *client;

  XrdWindow *switch_button;

  GulkanTexture *cursor_texture;
  guint64 click_source;
  guint64 move_source;
  guint64 keyboard_source;
  guint64 quit_source;
  guint render_source;
  bool shutdown;

  GdkPixbuf *window_pixbuf;
  GdkPixbuf *child_window_pixbuf;

  /* A real window manager will have a function like
   * _desktop_window_process_frame when updating a window's contents.
   * In this example, this is simulated by calling the window
   * update function for each desktop window in a imer. */
  GSList *desktop_window_list;
  gint64 desktop_window_manager_update_loop;
} Example;




/* Placeholder struct for a real desktop window manager's window struct.
 * Examples are KWin::EffectWindow (kwin plugin) and MetaWindow (gnome) */
typedef struct DesktopWindow
{
  GdkPixbuf *pixbuf;
  gchar *title;
} DesktopWindow;

/* In a real desktop window manager, the wm creates window structs, not us. */
static DesktopWindow *
_create_desktop_window (Example *self,
                        gchar *title,
                        GdkPixbuf *pixbuf)
{
  DesktopWindow *desktop_window = g_malloc (sizeof (DesktopWindow));
  desktop_window->pixbuf = pixbuf;
  desktop_window->title = title;
  self->desktop_window_list =
    g_slist_append (self->desktop_window_list, desktop_window);
  return desktop_window;
}

static gboolean
_desktop_window_process_frame (Example *self, DesktopWindow *desktop_window);

static gboolean
_desktop_window_manager_update_loop_cb (gpointer _self)
{
  Example *self = _self;
  for (GSList *l = self->desktop_window_list; l; l = l->next)
    {
      DesktopWindow *desktop_window = l->data;
      _desktop_window_process_frame (self, desktop_window);
    }
  return TRUE;
}

static gboolean
_sigint_cb (gpointer _self)
{
  Example *self = (Example*) _self;
  g_main_loop_quit (self->loop);
  return TRUE;
}

static GdkPixbuf *
load_gdk_pixbuf (const gchar* name)
{
  GError * error = NULL;
  GdkPixbuf *pixbuf_rgb = gdk_pixbuf_new_from_resource (name, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_rgb, false, 0, 0, 0);
  g_object_unref (pixbuf_rgb);

  return pixbuf;
}




/* Wapper around a native desktop window that will be stored in a XrdWindow's
 * "native" property.
 *
 * Store various window specific data here. For example useful:
 * - Reference to the native window the XrdWindow mirrors.
 * - Cache of the GulkanTexture to avoid allocating a new one every frame.
 * - with external memory: cache of the GL texture shared with GulkanTexture.
 * - Initial state of a desktop window, to restore when exiting VR mirror mode.
 *   e.g. if it is displayed always above other windows.
 */
typedef struct WindowWrapper
{
  DesktopWindow *desktop_window;
  GulkanTexture *gulkan_texture;
} WindowWrapper;

static GulkanTexture *
_desktop_window_to_gulkan_texture (Example *self,
                                   DesktopWindow *desktop_window)
{
  GulkanClient *gc = xrd_client_get_uploader (self->client);
  VkImageLayout layout = xrd_client_get_upload_layout (self->client);

  GulkanTexture *tex =
    gulkan_client_texture_new_from_pixbuf (gc,
                                           desktop_window->pixbuf,
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           layout,
                                           true);
  return tex;
}

static gboolean
_desktop_window_process_frame (Example *self, DesktopWindow *desktop_window)
{
  XrdWindow *xrd_window = xrd_client_lookup_window (self->client,
                                                    desktop_window);
  if (!xrd_window)
    {
      g_print ("Error processing frame, window is NULL\n");
      return TRUE;
    }

  WindowWrapper *window_wrapper;
  g_object_get (xrd_window, "native", &window_wrapper, NULL);

  /* it's important to always get the uploader from the client because
   * if the client is replaced, the previous uploader becomes invalid */
  GulkanClient *gc = xrd_client_get_uploader (self->client);

  guint window_texture_width =
    (guint)gdk_pixbuf_get_width (desktop_window->pixbuf);
  guint window_texture_height =
    (guint)gdk_pixbuf_get_height (desktop_window->pixbuf);

  /* a new Gulkan texture is needed if it has not been created (first use)
   * or if the desktop window's dimensions have changed, so the cached
   * texture's resolution doesn't match the desktop window anymore */
  if (window_wrapper->gulkan_texture == NULL ||
      window_texture_width !=
        gulkan_texture_get_width (window_wrapper->gulkan_texture) ||
      window_texture_height !=
        gulkan_texture_get_height (window_wrapper->gulkan_texture))
    {
      // g_print ("Allocating new texture for %s\n", desktop_window->title);
      if (window_wrapper->gulkan_texture)
        g_object_unref (window_wrapper->gulkan_texture);

      window_wrapper->gulkan_texture =
        _desktop_window_to_gulkan_texture (self, desktop_window);
    }
  else
    {
      /* update window_wrapper->gulkan_texture with desktop window content */
    }

  xrd_window_submit_texture (xrd_window, gc, window_wrapper->gulkan_texture);
  return TRUE;
}

static XrdWindow *
_add_window (Example *self,
             DesktopWindow *desktop_window,
             float width_meter,
             gboolean draggable)

{
  /* a real window manager needs to find these from real desktop windows. */
  gchar *title = desktop_window->title;
  guint texture_width = (guint)gdk_pixbuf_get_width (desktop_window->pixbuf);
  guint texture_height = (guint)gdk_pixbuf_get_height (desktop_window->pixbuf);

  float ppm = texture_width / width_meter;

  XrdWindow *window =
    xrd_client_window_new_from_pixels (self->client, title,
                                       texture_width, texture_height, ppm);

  WindowWrapper *native = g_malloc (sizeof (WindowWrapper));
  native->gulkan_texture = NULL;
  g_object_set (window, "native", native, NULL);

  xrd_client_add_window (self->client, window, draggable, desktop_window);

  return window;
}

static gboolean
_init_client (Example *self, XrdClient *client);
static void
_cleanup_client (Example *self);

static gboolean
perform_switch (gpointer data)
{
  Example *self = data;
  /* disconnect all event callbacks */
  _cleanup_client (self);

  /* gulkan textures become invalid in a new client instance */
  GSList *windows = xrd_client_get_windows (self->client);
  for (GSList *l = windows; l != NULL; l = l->next)
    {
      XrdWindow *window = l->data;
      WindowWrapper *native = NULL;
      g_object_get (window, "native", &native, NULL);
      g_clear_object (&native->gulkan_texture);
    }

  self->client = xrd_client_switch_mode (self->client);

  /* set up the example on the new client */
  _init_client (self, self->client);

  return FALSE;
}

static void
_button_switch_press_cb (XrdWindow               *window,
                         XrdControllerIndexEvent *event,
                         gpointer                 _self)
{
  (void) event;
  (void) window;
  Example *self = _self;

  /* Don't clean up bere because the callback will return.
   * Instead do the cleanup and switch on the next mainloop iteration. */
  g_timeout_add (0, perform_switch, self);
  g_free (event);
}

static void
_init_child_window (Example      *self,
                    XrdWindow    *window)
{
  DesktopWindow *desktop_window =
     _create_desktop_window (self, "A child", self->child_window_pixbuf);

  XrdWindow *child = _add_window (self, desktop_window, 0.25f, FALSE);

  graphene_point_t offset = { .x = 25, .y = 25 };
  xrd_window_add_child (window, child, &offset);
}

static gboolean
_init_cursor (Example *self, GulkanClient *gc)
{
  GdkPixbuf *cursor_pixbuf = load_gdk_pixbuf ("/res/cursor.png");
  if (cursor_pixbuf == NULL)
    {
      g_printerr ("Could not load image.\n");
      return FALSE;
    }

  VkImageLayout layout = xrd_client_get_upload_layout (self->client);

  self->cursor_texture =
    gulkan_client_texture_new_from_pixbuf (gc, cursor_pixbuf,
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           layout, true);

  xrd_client_submit_cursor_texture (self->client, gc,
                                    self->cursor_texture, 3, 3);

  g_object_unref (cursor_pixbuf);

  return TRUE;
}

static void
_init_buttons (Example *self)
{
  graphene_point3d_t switch_pos = {
    .x =  -0.75f,
    .y =  0.0f,
    .z = -1.0f
  };

  gchar *switch_str[] =  { "Switch", "Mode"};
  self->switch_button =
    xrd_client_button_new_from_text (self->client, 0.5f, 0.5f, 450.0f,
                                     2, switch_str);

  if (!self->switch_button)
    return;

  xrd_client_add_button (self->client, self->switch_button, &switch_pos,
                         (GCallback) _button_switch_press_cb, self);
}

static gboolean
_init_windows (Example *self)
{
  float window_x = 0;
  float window_y = 0;

  /* A window manager iterates over current windows and decides which of those
   * should be mirrored. This example creates a grid with placeholders. */
  for (int col = 0; col < GRID_WIDTH; col++)
    {
      float max_window_height = 0;
      for (int row = 0; row < GRID_HEIGHT; row++)
        {
          DesktopWindow *desktop_window =
             _create_desktop_window (self, "A window", self->window_pixbuf);

          XrdWindow *window = _add_window (self, desktop_window, 0.5f, TRUE);

          window_x += xrd_window_get_current_width_meters (window);

          float window_height = xrd_window_get_current_height_meters (window);
          if (window_height > max_window_height)
            max_window_height = window_height;

          graphene_point3d_t point = {
            .x = window_x,
            .y = window_y,
            .z = -3
          };
          graphene_matrix_t transform;
          graphene_matrix_init_translate (&transform, &point);
          xrd_window_set_transformation (window, &transform);

          xrd_window_save_reset_transformation (window);

          if ((col + row) % 2 == 0)
            xrd_window_set_flip_y (window, true);

          if (col == 0 && row == 0)
            _init_child_window (self, window);
        }
      window_x = 0;
      window_y += max_window_height;
    }

  return TRUE;
}

static void
_cleanup_client (Example *self)
{
  if (self->render_source != 0)
    g_source_remove (self->render_source);
  g_signal_handler_disconnect (self->client, self->click_source);
  g_signal_handler_disconnect (self->client, self->move_source);
  g_signal_handler_disconnect (self->client, self->keyboard_source);
  g_signal_handler_disconnect (self->client, self->quit_source);
  self->click_source = 0;
  self->move_source = 0;
  self->keyboard_source = 0;
  self->quit_source = 0;
  self->render_source = 0;
}

static void
_cleanup (Example *self)
{
  self->shutdown = true;

  _cleanup_client (self);

  GSList *windows = xrd_client_get_windows (self->client);
  for (GSList *l = windows; l; l = l->next)
    {
      XrdWindow *window = l->data;
      WindowWrapper *example_window;
      g_object_get (window, "native", &example_window, NULL);
      g_object_unref (example_window->gulkan_texture);

      xrd_window_close (window);
    }

  g_object_unref (self->window_pixbuf);
  g_object_unref (self->child_window_pixbuf);

  g_object_unref (self->cursor_texture);
  self->cursor_texture = NULL;
  g_object_unref (self->client);
  self->client = NULL;
  g_print ("Cleaned up!\n");
}

static void
_click_cb (XrdClient     *client,
           XrdClickEvent *event,
           Example       *self)
{
  (void) client;
  (void) self;
  g_print ("button %d %s at %f, %f\n",
           event->button, event->state ? "pressed" : "released",
           (double) event->position->x,
           (double) event->position->y);
}


static void
_move_cursor_cb (XrdClient          *client,
                 XrdMoveCursorEvent *event,
                 Example            *self)
{
  (void) client;
  (void) self;
  (void) event;
  /*
  g_print ("move: %f, %f\n",
           event->position->x, event->position->y);
   */
}


static void
_keyboard_press_cb (XrdClient   *client,
                    GdkEventKey *event,
                    Example     *self)
{
  (void) client;
  (void) self;
  g_print ("key: %d\n", event->keyval);
}

static void
_request_quit_cb (XrdClient *client,
                  OpenVRQuitEvent *event,
                  Example   *self)
{
  (void) client;
  (void) self;

  switch (event->reason)
  {
    case VR_QUIT_SHUTDOWN:
    {
      g_print ("Quit event: Shutdown\n");
      g_main_loop_quit (self->loop);
    } break;
    case VR_QUIT_PROCESS_QUIT:
    {
      g_print ("Quit event: Process quit\n");
      if (XRD_IS_OVERLAY_CLIENT (self->client))
        g_timeout_add (0, perform_switch, self);
    } break;
    case VR_QUIT_APPLICATION_TRANSITION:
    {
      g_print ("Quit event: Application transition\n");
      if (XRD_IS_SCENE_CLIENT (self->client))
        g_timeout_add (0, perform_switch, self);
    } break;
  }
}

static gboolean
_iterate_cb (gpointer _self)
{
  Example *self = (Example*) _self;

  if (self->shutdown)
    return FALSE;

  xrd_scene_client_render (XRD_SCENE_CLIENT (self->client));
  return TRUE;
}

static gboolean
_init_client (Example *self, XrdClient *client)
{
  if (!client)
    {
      g_printerr ("XrdClient did not initialize correctly.\n");
      return FALSE;
    }

  self->click_source = g_signal_connect (client, "click-event",
                                         (GCallback) _click_cb, self);
  self->move_source = g_signal_connect (client, "move-cursor-event",
                                        (GCallback) _move_cursor_cb, self);
  self->keyboard_source = g_signal_connect (client, "keyboard-press-event",
                                            (GCallback) _keyboard_press_cb,
                                            self);
  self->quit_source = g_signal_connect (client, "request-quit-event",
                                        (GCallback) _request_quit_cb, self);

  self->render_source = 0;
  if (XRD_IS_SCENE_CLIENT (client))
    self->render_source = g_timeout_add (1, _iterate_cb, self);

  GulkanClient *gc = xrd_client_get_uploader (client);
  if (!_init_cursor (self, gc))
    return FALSE;

  _init_buttons (self);

  return TRUE;
}

static gboolean
_init_example (Example *self, XrdClient *client)
{

  self->client = client;

  /* TODO: remove special case */
  if (XRD_IS_SCENE_CLIENT (client))
    if (!xrd_scene_client_initialize (XRD_SCENE_CLIENT (client)))
      return FALSE;

  self->shutdown = false;

  if (!_init_client (self, client))
    return FALSE;

  if (!_init_windows (self))
    return FALSE;

  g_unix_signal_add (SIGINT, _sigint_cb, self);

  self->desktop_window_manager_update_loop =
    g_timeout_add (16, _desktop_window_manager_update_loop_cb, self);

  return TRUE;
}

static gboolean overlay = FALSE;
static gboolean automatic = FALSE;

static GOptionEntry entries[] =
{
  { "overlay", 'o', 0, G_OPTION_ARG_NONE, &overlay,
      "Launch overlay client by default.", NULL },
  { "auto", 'a', 0, G_OPTION_ARG_NONE, &automatic,
      "Launch overlay client if another scene app is already running,\n"
      "else launch scene client.", NULL },
};

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("- xrdesktop client example.");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Wrong parameters: %s\n", error->message);
      g_option_context_free (context);
      exit (1);
    }
  g_option_context_free (context);

  Example self = {
    .loop = g_main_loop_new (NULL, FALSE),
    .window_pixbuf = load_gdk_pixbuf ("/res/hawk.jpg"),
    .child_window_pixbuf = load_gdk_pixbuf ("/res/cat.jpg"),
    .desktop_window_list = NULL,
  };


  gboolean scene_available = !openvr_context_is_another_scene_running ();

  XrdClient *client;
  if (automatic)
    {
      if (scene_available)
        client = XRD_CLIENT (xrd_scene_client_new ());
      else
        client = XRD_CLIENT (xrd_overlay_client_new ());
    }
  else
    {
      if (overlay)
        client = XRD_CLIENT (xrd_overlay_client_new ());
      else
        {
          if (scene_available)
              client = XRD_CLIENT (xrd_scene_client_new ());
          else
            {
              g_print ("Not starting xrdesktop in scene mode, because another "
                       "scene app is already running\n");
              return 1;
            }
        }
    }


  if (!_init_example (&self, client))
    return 1;

  /* start glib main loop */
  g_main_loop_run (self.loop);

  /* don't clean up when quitting during switching */
  if (self.client != NULL)
    _cleanup (&self);

  g_main_loop_unref (self.loop);
}
