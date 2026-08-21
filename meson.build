project(
    'power-control',
    'cpp',
    default_options: ['warning_level=3', 'cpp_std=c++23'],
    license: 'Apache-2.0',
    version: '0.1',
    meson_version: '>=1.1.1',
)

add_project_arguments(
    '-Wno-psabi',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_ASIO_DISABLE_THREADS',
    language: 'cpp',
)

cpp_args = []

if get_option('chassis-system-reset').allowed()
    cpp_args += '-DCHASSIS_SYSTEM_RESET'
endif
if get_option('use-plt-rst').allowed()
    cpp_args += '-DUSE_PLT_RST'
endif
if get_option('use-acboot').allowed()
    cpp_args += '-DUSE_ACBOOT'
endif
if get_option('ignore-soft-resets-during-post').allowed()
    cpp_args += '-DIGNORE_SOFT_RESETS_DURING_POST'
endif
if get_option('button-passthrough').allowed()
    cpp_args += '-DUSE_BUTTON_PASSTHROUGH'
endif

if get_option('tests').enabled()
    add_project_arguments('-DUNIT_TESTING', '-Wno-parentheses', language: 'cpp')
    sdbusplus = subproject('sdbusplus').get_variable('sdbusplus_dep')
    meson.override_dependency('sdbusplus', sdbusplus)
    phosphor_dbus_interfaces_dep = subproject('phosphor-dbus-interfaces').get_variable('phosphor_dbus_interfaces_dep')
    phosphor_logging_dep = subproject('phosphor-logging').get_variable('phosphor_logging_dep')
    meson.override_dependency('phosphor-logging', phosphor_logging_dep)
endif

deps = [
    dependency('libgpiodcxx', default_options: ['bindings=cxx']),
    dependency('libsystemd'),
    dependency('sdbusplus'),
    dependency('phosphor-logging'),
    dependency('nlohmann_json', include_type: 'system'),
    dependency('boost'),
]

if get_option('tests').enabled()
    pdkhook_deps = [dependency('libpdkhook', fallback: ['libpdkhook', 'libpdkhook_dep'])]
else
    pdkhook_deps = [declare_dependency(link_args: '-lpdkhook')]
endif

executable(
    'power-control',
    'src/power_control.cpp',
    include_directories: include_directories('src'),
    cpp_args: cpp_args,
    dependencies: [deps, pdkhook_deps],
    install: true,
    install_dir: get_option('bindir'),
)

systemd = dependency('systemd')
if systemd.found()
    install_data(
        'service_files/chassis-system-reset.service',
        'service_files/chassis-system-reset.target',
        'service_files/xyz.openbmc_project.Chassis.Control.Power@.service',
        install_dir: systemd.get_variable('systemd_system_unit_dir'),
    )
endif

install_data(
    'config/power-config-host0.json',
    install_dir: '/usr/share/x86-power-control/',
)

if get_option('tests').enabled()
    subdir('tests')
endif
