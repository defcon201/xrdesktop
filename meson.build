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

# Paths
xrdesktop_prefix = get_option('prefix')
xrdesktop_libdir = join_paths(xrdesktop_prefix, get_option('libdir'))
xrdesktop_includedir = join_paths(xrdesktop_prefix, get_option('includedir'))
xrdesktop_datadir = join_paths(xrdesktop_prefix, get_option('datadir'))
src_inc = include_directories('src')

### Dependencies
c = meson.get_compiler('c')

## Required
m_dep = c.find_library('m')
openvr_glib_dep = dependency('openvr-glib-0.9', version: '>= 0.9.0')

subdir('shaders')
subdir('res')
subdir('src')
subdir('tests')
subdir('examples')
subdir('doc')
