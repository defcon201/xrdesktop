executable(
  'overlay_paint', ['overlay_paint.c', test_resources],
  dependencies: [gtk3_dep, openvr_dep, cairo_dep, graphene_dep, gulkan_dep, openvr_glib_dep],
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)

executable(
  'overlay_models', ['overlay_models.c', test_resources],
  dependencies: [gtk3_dep, openvr_dep, cairo_dep, graphene_dep, gulkan_dep, openvr_glib_dep],
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)

executable(
  'overlay_management', ['overlay_management.c', test_resources],
  dependencies: [gtk3_dep, openvr_dep, cairo_dep, graphene_dep, m_dep,
                 gulkan_dep, openvr_glib_dep],
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)

executable(
  'overlay_client', ['overlay_client.c', test_resources],
  dependencies: [gtk3_dep, openvr_dep, cairo_dep, graphene_dep, m_dep,
                 gulkan_dep, openvr_glib_dep],
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)

executable(
  'scene', ['scene.c', test_resources, shader_resources],
  dependencies: [gdk_pixbuf_dep, gio_dep, glfw_dep,
                 graphene_dep, gulkan_dep, cairo_dep, openvr_glib_dep],
  include_directories: [xrdesktop_inc],
  link_with: xrdesktop_lib,
  install: false)