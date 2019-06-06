# Tests without XR

test_math_matrix_decomposition = executable(
  'test_math_matrix_decomposition', 'test_math_matrix_decomposition.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_math_matrix_decomposition', test_math_matrix_decomposition)

test_math_angles = executable(
  'test_math_angles', 'test_math_angles.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  c_args : ['-DXRD_COMPILATION'],
  install: false)
test('test_math_angles', test_math_angles)

test_gsettings = executable(
  'test_gsettings', 'test_gsettings.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_gsettings', test_gsettings, suite: 'post-install')

# Tests with XR

test_scene_client = executable(
  'test_scene_client', ['test_scene_client.c', test_resources,
                        shader_resources],
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_scene_client', test_scene_client, suite: ['xr', 'post-install'])

test_overlay_client = executable(
  'test_overlay_client', ['test_overlay_client.c', test_resources],
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_overlay_client', test_overlay_client, suite: ['xr', 'post-install'])

test_window = executable(
  'test_window', 'test_window.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_window', test_window, suite: 'xr')


