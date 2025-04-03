api_version = '0.1'

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])

cpc_gpu_version_file = configure_file(
          input: 'cpc-gpu-version.h.in',
         output: 'cpc-gpu-version.h',
  configuration: version_conf,
        install: true,
    install_dir: get_option('includedir') / 'cpc-gpu'
)

cpc_gpu_sources = [
  'cpc-gpu.c',
  'cpc-gpu-util.c',
  'cpc-gpu-gl.c',
  # 'cpc-gpu-vk.c',
]

cpc_gpu_headers = [
  cpc_gpu_version_file,
  'cpc-gpu-version-macros.h',
  'cpc-gpu.h',
]

cpc_gpu_c_args = [
  project_c_args,
  '-DG_LOG_DOMAIN="CpcGpu"',
  '-DCPC_GPU_COMPILATION',
]
cpc_gpu_deps = [dependency('glib-2.0')]
pkg_requires = ['glib-2.0']

if get_option('epoxy')
  cpc_gpu_c_args += ['-DUSE_EPOXY']
  cpc_gpu_deps += [dependency('epoxy')]
endif

if get_option('gobject')
  cpc_gpu_sources += ['cpc-gpu-gobject.c']
  cpc_gpu_headers += ['cpc-gpu-gobject.h']
  cpc_gpu_deps += [dependency('gobject-2.0')]
  pkg_requires += ['gobject-2.0']
endif

cpc_gpu_lib = library('cpc-gpu-' + api_version,
  cpc_gpu_sources,
  dependencies: cpc_gpu_deps,
        c_args: cpc_gpu_c_args,
       install: true,
)

install_headers(cpc_gpu_headers, subdir: 'cpc-gpu')

pkg = import('pkgconfig')
pkg.generate(
  description: 'GPU module for Carapace',
    libraries: cpc_gpu_lib,
         name: 'cpc-gpu',
     filebase: 'cpc-gpu-' + api_version,
      version: meson.project_version(),
      subdirs: 'cpc-gpu',
     requires: pkg_requires,
  install_dir: get_option('libdir') / 'pkgconfig'
)

cpc_gpu_dep = declare_dependency(
  link_with : cpc_gpu_lib,
  dependencies : cpc_gpu_deps,
  sources : [cpc_gpu_version_file],
  include_directories : include_directories('..', '.'))
