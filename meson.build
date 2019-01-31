project('xrdesktop', 'c', version: '0.9.0',
  meson_version: '>= 0.42.0',
)

gnome = import('gnome')

config_h = configuration_data()
configure_file(
  output: 'xrdesktop-config.h',
  configuration: config_h,
)
add_project_arguments([
  ['-I' + meson.build_root(),
   #'-Werror',
   '-Wall',
   '-Wextra'
],
], language: ['c'])

### Dependencies
c = meson.get_compiler('c')

## Required
m_dep = c.find_library('m')
gulkan_dep = dependency('gulkan-0.9', version: '>= 0.9.0')
openvr_glib_dep = dependency('openvr-glib-0.9', version: '>= 0.9.0')

gio_dep = dependency('gio-2.0', version: '>= 2.50')
gdk_dep = dependency('gdk-3.0', version: '>= 3.22')
cairo_dep = dependency('cairo')

gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36')
vulkan_dep = dependency('vulkan')
graphene_dep = dependency('graphene-1.0')

subdir('shaders')
subdir('res')
subdir('src')
subdir('tests')
subdir('examples')