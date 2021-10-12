project('ibus-m17n', 'c',
  version: '1.4.7',
  meson_version: '>= 0.53',
)

# Modules
gnome = import('gnome')
i18n = import('i18n')

# Some variables
po_dir = meson.current_source_dir() / 'po'
config_h_inc = include_directories('.')

# Dependencies
ibus_dep = dependency('ibus-1.0', version: '>+ 1.4')
m17n_dep = dependency('m17n-shell')
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')

if get_option('gtk') == '2'
  gtk_dep = dependency('gtk+-2.0', version: '>= 2.12.12')
elif
  gtk_dep = dependency('gtk+-3.0', version: '>= 2.90.5')
else
  error('Unsupported value for \'gtk\' option')
endif

# config.h
conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set10('HAVE_SETUP', get_option('gtk') != 'disabled')
configure_file(output: 'config.h', configuration: conf)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

# Appdata
install_data('m17n.appdata.xml',
  install_dir: get_option('datadir') / 'metainfo',
)

# RPM spec
ibus_m17n_rpm_spec = configure_file(
  input: 'ibus-m17n.spec.in',
  output: '@BASENAME@',
  configuration: {
    'VERSION': meson.project_version(),
    'PACKAGE_VERSION': meson.project_version(),
  },
)

# Subdirectories
subdir('src')
subdir('icons')
subdir('po')
