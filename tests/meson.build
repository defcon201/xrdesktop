test_system = executable(
  'test_scene_client', 'test_scene_client.c',
  dependencies: xrdesktop_deps,
  link_with: xrdesktop_lib,
  include_directories: xrdesktop_inc,
  install: false)
test('test_scene_client', test_system)
