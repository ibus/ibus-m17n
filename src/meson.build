m17ncommon_cflags = [
  '-DPKGDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir') / meson.project_name()),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
]

# Little helper library to implement common functionality
m17ncommon_lib = static_library('m17ncommon',
  files('m17nutil.c'),
  dependencies: ibus_dep,
  c_args: m17ncommon_cflags,
  include_directories: config_h_inc,
)

# Engine
ibus_engine_m17n_sources = files(
  'main.c',
  'engine.c',
)

ibus_engine_m17n_deps = [
  ibus_dep,
  m17n_dep,
]

executable('ibus-engine-m17n',
  ibus_engine_m17n_sources,
  dependencies: ibus_engine_m17n_deps,
  include_directories: config_h_inc,
  link_with: m17ncommon_lib,
  c_args: m17ncommon_cflags,
  install: true,
  install_dir: get_option('libexecdir'),
)

# IBus components
configure_file(
  input: 'm17n.xml.in',
  output: '@BASENAME@',
  configuration: {
    'VERSION': meson.project_version(),
    'libexecdir': get_option('prefix') / get_option('libexecdir'),
    'pkgdatadir': get_option('prefix') / get_option('datadir') / meson.project_name(),
  },
  install: true,
  install_dir: get_option('datadir') / 'ibus' / 'component',
)

install_data('default.xml',
  install_dir: get_option('datadir') / 'ibus' / 'component',
)

# UI setup
if get_option('gtk') != 'disabled'
  ibus_setup_m17n_sources = files(
    'setup.c',
  )

  ibus_setup_m17n_deps = [
    gio_dep,
    gtk_dep,
    ibus_dep,
    m17n_dep,
  ]

  ibus_setup_m17n = executable('ibus-setup-m17n',
    ibus_setup_m17n_sources,
    include_directories: config_h_inc,
    dependencies: ibus_setup_m17n_deps,
    link_with: m17ncommon_lib,
    c_args: m17ncommon_cflags,
  )

  install_data('ibus-m17n-preferences.ui',
    install_dir: get_option('datadir') / meson.project_name() / 'setup',
  )

  desktop_file = i18n.merge_file(
    input: configure_file(
      input: 'ibus-setup-m17n.desktop.in.in',
      output: '@BASENAME@',
      configuration: {
        'libexecdir': get_option('prefix') / get_option('libexecdir'),
      },
    ),
    output: '@BASENAME@',
    type: 'desktop',
    po_dir: po_dir,
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )
endif

# GSchema
install_data('org.freedesktop.ibus.engine.m17n.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Tests
ibus_m17n_test_bin = executable('test-m17n',
  files('test.c'),
  dependencies: [
    ibus_dep,
    m17n_dep,
  ],
  link_with: m17ncommon_lib,
  include_directories: config_h_inc,
)

test('m17n',
  ibus_m17n_test_bin,
  env: [
    'IBUS_M17N_PKGDATADIR=@0@'.format(meson.current_build_dir()),
  ],
)
