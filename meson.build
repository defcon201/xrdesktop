project('xrdesktop', 'c', version: '0.8.99',
  meson_version: '>= 0.42.0',
)

gnome = import('gnome')

config_h = configuration_data()
configure_file(
  output: 'openvr-glib-config.h',
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
cpp = meson.get_compiler('c')

## Required
openvr_dep = cpp.find_library('openvr_api')

# For a statically linked libopenvr_api we also need to link to the
# Standard C++ library and the implementation of dlopen()
stdcpp_dep = cpp.find_library('stdc++')
dl_dep = cpp.find_library('dl')

m_dep = cpp.find_library('m')
gulkan_dep = dependency('gulkan-0.8', version: '>= 0.8.0')
openvr_glib_dep = dependency('openvr-glib-0.9', version: '>= 0.8.0')

gio_dep = dependency('gio-2.0', version: '>= 2.50')
gtk3_dep = dependency('gtk+-3.0', version: '>= 3.22')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36')
vulkan_dep = dependency('vulkan')
graphene_dep = dependency('graphene-1.0')

## Optional
x11_dep = dependency('x11', required : false)
xtst_dep = dependency('xtst', required : false)
xi_dep = dependency('xi', required : false)
clutter_gtk_dep = dependency('clutter-gtk-1.0', required : false)
clutter_dep = dependency('clutter-1.0', required : false)
cairo_dep = dependency('cairo', required : false)
libdrm_dep = dependency('libdrm', required : false)
libdrm_amdgpu_dep = dependency('libdrm_amdgpu', required : false)
glew_dep = dependency('glew', required : false)
glfw_dep = dependency('glfw3', required : false)
egl_dep = dependency('egl', required : false)
###

subdir('shaders')
subdir('src')
subdir('res')
subdir('tests')
subdir('examples')
