so_version = 0
api_version = '0.9'
api_path = 'xrdesktop-' + api_version

xrdesktop_sources = [
  'xrd-overlay-model.c',
  'xrd-overlay-pointer-tip.c',
  'xrd-overlay-desktop-cursor.c',
  'xrd-overlay-pointer.c',
  'xrd-window-manager.c',
  'xrd-overlay-client.c',
  'xrd-overlay-window.c',
  'xrd-window.c',
  'xrd-follow-head-container.c',
  'xrd-scene-model.c',
  'xrd-scene-device-manager.c',
  'xrd-scene-device.c',
  'xrd-scene-client.c',
  'xrd-scene-window.c',
  'xrd-scene-pointer.c',
  'xrd-scene-object.c',
  'xrd-scene-selection.c',
  'xrd-settings.c',
  'xrd-input-synth.c',
  'xrd-math.c',
  binding_resources
]

xrdesktop_headers = [
  'xrd.h',
  'xrd-overlay-model.h',
  'xrd-overlay-pointer-tip.h',
  'xrd-overlay-desktop-cursor.h',
  'xrd-overlay-pointer.h',
  'xrd-window-manager.h',
  'xrd-overlay-client.h',
  'xrd-overlay-window.h',
  'xrd-window.h',
  'xrd-follow-head-container.h',
  'xrd-scene-model.h',
  'xrd-scene-device-manager.h',
  'xrd-scene-device.h',
  'xrd-scene-client.h',
  'xrd-scene-window.h',
  'xrd-scene-pointer.h',
  'xrd-scene-object.h',
  'xrd-scene-selection.h',
  'xrd-settings.h',
  'xrd-input-synth.h',
  'xrd-math.h',
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

xrdesktop_lib = shared_library(api_path,
  xrdesktop_sources,
  version: meson.project_version(),
  soversion: so_version,
  dependencies: xrdesktop_deps,
  install: true,
)

xrdesktop_inc = include_directories('.')

xrdesktop_dep = declare_dependency(
  sources: [],
  link_with: xrdesktop_lib,
  include_directories: [ xrdesktop_inc ],
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
