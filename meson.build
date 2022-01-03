project(
  'seogi',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
    'werror=true',
  ],
)

cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([ '-Wno-unused-parameter' ]),
  language: 'c'
)

src_files = files(
  'src/dbus.c',
  'src/event-loop.c',
  'src/key-buffer.c',
  'src/main.c',
  'src/seat.c',
  'src/utf8.c'
)

include_dirs = include_directories('include')

libhangul = dependency('libhangul')
libsystemd = dependency('libsystemd')
wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols')
xkbcommon = dependency('xkbcommon')

subdir('protocol')

executable(
  meson.project_name(),
  src_files,
  include_directories: include_dirs,
  dependencies: [libhangul, libsystemd, wayland_client, xkbcommon],
  install: true,
)
