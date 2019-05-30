test_scene_client = executable(
  'test_scene_client', ['test_scene_client.c', test_resources,
                        shader_resources],
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_scene_client', test_scene_client)

test_overlay_client = executable(
  'test_overlay_client', ['test_overlay_client.c', test_resources],
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_overlay_client', test_overlay_client)

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
