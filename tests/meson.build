test_system = executable(
  'test_scene_client', 'test_scene_client.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_scene_client', test_system)

test_window = executable(
  'test_window', 'test_window.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_window', test_window)

test_math_matrix_decomposition = executable(
  'test_math_matrix_decomposition', 'test_math_matrix_decomposition.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_math_matrix_decomposition', test_math_matrix_decomposition)
