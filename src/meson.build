so_version = 0
api_version = '0.11'
api_path = 'xrdesktop-' + api_version

xrdesktop_sources = [
  'xrd-controller.c',
  'xrd-window-manager.c',
  'xrd-window.c',
  'xrd-container.c',
  'xrd-settings.c',
  'xrd-input-synth.c',
  'xrd-shake-compensator.c',
  'xrd-client.c',
  'xrd-math.c',
  'xrd-pointer.c',
  'xrd-pointer-tip.c',
  'xrd-desktop-cursor.c',
  'graphene-ext.c',
  'scene/xrd-scene-model.c',
  'scene/xrd-scene-device-manager.c',
  'scene/xrd-scene-device.c',
  'scene/xrd-scene-client.c',
  'scene/xrd-scene-window.c',
  'scene/xrd-scene-pointer.c',
  'scene/xrd-scene-object.c',
  'scene/xrd-scene-selection.c',
  'scene/xrd-scene-vector.c',
  'scene/xrd-scene-background.c',
  'scene/xrd-scene-pointer-tip.c',
  'scene/xrd-scene-desktop-cursor.c',
  'scene/xrd-scene-renderer.c',
  'overlay/xrd-overlay-model.c',
  'overlay/xrd-overlay-pointer-tip.c',
  'overlay/xrd-overlay-desktop-cursor.c',
  'overlay/xrd-overlay-pointer.c',
  'overlay/xrd-overlay-client.c',
  'overlay/xrd-overlay-window.c',
  binding_resources,
  icon_resources
]

xrdesktop_headers = [
  'xrd.h',
  'xrd-controller.h',
  'xrd-window-manager.h',
  'xrd-window.h',
  'xrd-container.h',
  'xrd-settings.h',
  'xrd-input-synth.h',
  'xrd-shake-compensator.h',
  'xrd-client.h',
  'xrd-math.h',
  'xrd-pointer.h',
  'xrd-pointer-tip.h',
  'xrd-desktop-cursor.h',
  'graphene-ext.h',
  'scene/xrd-scene-model.h',
  'scene/xrd-scene-device-manager.h',
  'scene/xrd-scene-device.h',
  'scene/xrd-scene-client.h',
  'scene/xrd-scene-window.h',
  'scene/xrd-scene-pointer.h',
  'scene/xrd-scene-object.h',
  'scene/xrd-scene-selection.h',
  'scene/xrd-scene-vector.h',
  'scene/xrd-scene-background.h',
  'scene/xrd-scene-pointer-tip.h',
  'scene/xrd-scene-desktop-cursor.h',
  'scene/xrd-scene-renderer.h',
  'overlay/xrd-overlay-model.h',
  'overlay/xrd-overlay-pointer-tip.h',
  'overlay/xrd-overlay-desktop-cursor.h',
  'overlay/xrd-overlay-pointer.h',
  'overlay/xrd-overlay-client.h',
  'overlay/xrd-overlay-window.h',
]

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', MAJOR_VERSION)
version_conf.set('MINOR_VERSION', MINOR_VERSION)
version_conf.set('MICRO_VERSION', MICRO_VERSION)

configure_file(
  input: 'xrd-version.h.in',
  output: 'xrd-version.h',
  configuration: version_conf,
  install: true,
  install_dir: join_paths(get_option('includedir'), api_path)
)

xrdesktop_deps = [
  m_dep,
  openvr_glib_dep
]

xrdesktop_inc = include_directories('.', 'scene', 'overlay')

xrdesktop_lib = shared_library(api_path,
  xrdesktop_sources + shader_resources,
  version: meson.project_version(),
  soversion: so_version,
  dependencies: xrdesktop_deps,
  include_directories: xrdesktop_inc,
  install: true,
  c_args : ['-DXRD_COMPILATION']
)

xrdesktop_dep = declare_dependency(
  sources: [],
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  dependencies: xrdesktop_deps,
)

install_headers(xrdesktop_headers, subdir: api_path)

pkg = import('pkgconfig')

pkg.generate(
  description: 'A library for XR interaction with classical desktop compositors.',
    libraries: xrdesktop_lib,
         name: 'xrdesktop',
     filebase: api_path,
      version: meson.project_version(),
      subdirs: api_path,
     requires: 'openvr-glib-' + api_version,
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)

if get_option('introspection')
  xrdesktop_gir = gnome.generate_gir(
    xrdesktop_lib,
    sources: xrdesktop_sources + xrdesktop_headers,
    namespace: 'Xrd',
    nsversion: api_version,
    identifier_prefix: 'Xrd',
    symbol_prefix: 'xrd',
    export_packages: api_path,
    includes: [ 'cairo-1.0', 'GdkPixbuf-2.0', 'Gdk-3.0'],
    header: 'xrdesktop/xrd.h',
    install: true,
  )
endif
