gtest_dep = dependency('gtest', main: false, required: false)
gmock_dep = dependency('gmock', required: false)

if not gtest_dep.found() or not gmock_dep.found()
    cmake = import('cmake')
    opt_var = cmake.subproject_options()
    opt_var.add_cmake_defines({'CMAKE_CXX_FLAGS': '-Wno-pedantic'})
    opt_var.set_override_option('warning_level', '3')
    opt_var.set_override_option('werror', 'true')

    gtest_proj = cmake.subproject('gtest', options: opt_var, required: true)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [dependency('threads'), gtest_proj.dependency('gtest')]
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(
            not get_option('tests').enabled(),
            'Googletest is required if tests are enabled',
        )
    endif
endif

production_sources = ['../src/power_control.cpp']

common_deps = [
    gtest_dep,
    gmock_dep,
    dependency('libgpiodcxx', default_options: ['bindings=cxx']),
    dependency('libsystemd'),
    dependency('sdbusplus'),
    dependency('phosphor-logging'),
    dependency('nlohmann_json', include_type: 'system'),
    dependency('boost'),
    dependency('libpdkhook', fallback: ['libpdkhook', 'libpdkhook_dep']),
]

common_cpp_args = [
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DUNIT_TESTING',
    '-Wno-psabi',
]

common_link_args = [
    '-lcrypt',
    '-lstdc++fs',
    '-Wl,--allow-multiple-definition',
]

# PersistentState() creates /var/lib/power-control at static-init time.
# Pre-create it; try unprivileged first, fall back to passwordless sudo.
setup_state_dir = custom_target(
    'setup_state_dir',
    command: [
        'sh', '-c',
        'mkdir -p /var/lib/power-control 2>/dev/null || sudo -n mkdir -p /var/lib/power-control',
    ],
    output: 'state_dir.stamp',
    capture: true,
    build_always_stale: true,
)

unit_test_exe = executable(
    'unit_test_power_control',
    [
        'test_main.cpp',
        'test_power_control.cpp',
    ] + production_sources,
    include_directories: include_directories('..', '../src'),
    dependencies: common_deps,
    cpp_args: common_cpp_args,
    link_args: common_link_args,
)

test(
    'unit_test_power_control',
    unit_test_exe,
    depends: setup_state_dir,
)
