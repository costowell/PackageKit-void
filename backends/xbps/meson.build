xbps_dep = dependency('libxbps', version: '>=0.59.0')

shared_module(
  'pk_backend_xbps',
  'pk-backend-xbps.c',
  include_directories: packagekit_src_include,
  dependencies: [
    packagekit_glib2_dep,
    gmodule_dep,
    xbps_dep
  ],
  c_args: [
    '-DG_LOG_DOMAIN="PackageKit-XBPS"',
  ],
  install: true,
  install_dir: pk_plugin_dir,
)
