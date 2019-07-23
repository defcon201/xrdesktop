project('xrdesktop', 'c', version: '0.12.0',
  meson_version: '>= 0.45.1',
)

gnome = import('gnome')

config_h = configuration_data()
configure_file(
  output: 'xrdesktop-config.h',
  configuration: config_h,
)

project_args = ['-I' + meson.build_root(),
   #'-Werror',
   '-Wall',
   '-Wextra',
]

compiler = meson.get_compiler('c')
compiler_id = compiler.get_id()

if compiler_id == 'clang'
  project_args += [
    '-Weverything',
    '-Wno-reserved-id-macro',
    '-Wno-documentation',
    '-Wno-documentation-unknown-command',
    '-Wno-padded',
    '-Wno-overlength-strings',
    '-Wno-disabled-macro-expansion',
    '-Wno-atomic-implicit-seq-cst',
    '-Wno-float-equal',
    '-Wno-used-but-marked-unused',
    '-Wno-assign-enum',
    '-Wno-gnu-folding-constant',
  ]
endif

add_project_arguments([project_args], language: ['c'])


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
gxr_dep = dependency('gxr-0.12')

subdir('shaders')
subdir('res')
subdir('src')
subdir('tests')
subdir('examples')
subdir('doc')
