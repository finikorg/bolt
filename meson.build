project('bolt',
  'c',
  version: '0.1',
  license : 'LGPL-2.1+',
  meson_version: '>= 0.42.0',
  default_options: ['warning_level=1',
                    'c_std=gnu99',
                    'buildtype=debugoptimized'])


# additional compiler warnings, if supported
test_args = [
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]

compiler = meson.get_compiler('c')

foreach arg: test_args
  if compiler.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# build options

build_man = get_option('man')
req_man = build_man == 'true'

# dependencies

gnome  = import('gnome')

glib    = dependency('glib-2.0', version: '>= 2.54.0')
gio     = dependency('gio-2.0')
libudev = dependency('libudev')
unix    = dependency('gio-unix-2.0')
udev    = dependency('udev')
umock   = dependency('umockdev-1.0')
polkit  = dependency('polkit-gobject-1')
systemd = dependency('systemd')

git     = find_program('git', required: false)
a2x     = find_program(['a2x', 'a2x.py'], required: req_man)

# configuration & well known directories

prefixdir = get_option('prefix')

srcdir = meson.source_root()
bindir = join_paths(prefixdir, get_option('bindir'))
libexecdir = join_paths(prefixdir, get_option('libexecdir'))
datadir = join_paths(prefixdir, get_option('datadir'))
sysconfdir = get_option('sysconfdir')
mandir = get_option('mandir')

dbdir = get_option('db-path')
udevdir = udev.get_pkgconfig_variable('udevdir')
unitdir = systemd.get_pkgconfig_variable('systemdsystemunitdir')

version_split = meson.project_version().split('.')
version_major = version_split[0]
version_minor = version_split[1]

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('DATADIR', datadir)
conf.set_quoted('BOLT_DBDIR', dbdir)

conf.set('VERSION_MAJOR', version_major)
conf.set('VERSION_MINOR', version_minor)

conf.set('_GNU_SOURCE', true)

foreach fn : [
  ['explicit_bzero', '''#include <string.h>'''],
  ['getrandom',      '''#include <sys/random.h>''']
]

  have = compiler.has_function(fn[0], prefix : fn[1])
  conf.set10('HAVE_FN_' + fn[0].to_upper(), have)
endforeach

if polkit.version().version_compare('>= 0.114')
  conf.set('HAVE_POLKIT_AUTOPTR', '1')
endif

config_h = configure_file(output: 'config.h', configuration: conf)

cargs = ['-DG_LOG_DOMAIN="Bolt"']

privileged_group = get_option('privileged-group')
subs = configuration_data()
subs.set('dbdir', dbdir)
subs.set('libexecdir', libexecdir)
subs.set('privileged_group', privileged_group)
subs.set('version', meson.project_version())

longdate = run_command('date', '+%a %b %d %Y').stdout().strip()
subs.set('longdate', longdate)

subs.set('reltag', 'pre1')
if git.found()
  gitres = run_command(git, ['--git-dir=@0@/.git'.format(srcdir),
                             'rev-parse',
                             '--short=7',
                             'HEAD'])
  if gitres.returncode() == 0
    gitrev = gitres.stdout().strip()
    gitdate = run_command('date', '+%Y%m%d').stdout().strip()
    # YYYYMMDD<scm><revision>
    subs.set('reltag', '@0@git@1@'.format(gitdate, gitrev))
  endif
endif

# common static library
#  contains code shared by daemon, command line tools
common_deps = [glib, gio, libudev, unix]

common_headers = [
  'common/bolt-enums.h',
  'common/bolt-error.h'
]

common_sources = [
  'common/bolt-enums.c',
  'common/bolt-error.c',
  'common/bolt-fs.c',
  'common/bolt-io.c',
  'common/bolt-rnd.c',
  'common/bolt-str.c',
  'common/bolt-term.c'
]

common_enums = gnome.mkenums_simple('bolt-enum-types',
  sources: common_headers)

gen_sources = [common_enums[0]]
gen_headers = [common_enums[1]]

common_lib = static_library('common',
  c_args : [cargs],
  sources: common_sources + gen_sources + gen_headers,
  dependencies: common_deps,
  include_directories: [
    include_directories('.')
])

common = declare_dependency(
  sources: gen_headers,
  dependencies: common_deps,
  link_with: common_lib,
  include_directories: [
    include_directories('common')
])


# boltd - the main daemon
daemon_sources = files([
  'boltd/bolt-auth.c',
  'boltd/bolt-bouncer.c',
  'boltd/bolt-manager.c',
  'boltd/bolt-device.c',
  'boltd/bolt-key.c',
  'boltd/bolt-store.c'
])

daemon_sources += gnome.gdbus_codegen('bolt-gdbus',
  'data/org.freedesktop.bolt.xml',
  interface_prefix: 'org.freedesktop.bolt1.',
  namespace: 'BoltDBus')

install_data(['data/org.freedesktop.bolt.xml'],
  install_dir : join_paths(datadir, 'dbus-1', 'interfaces')
)

install_data(['data/org.freedesktop.bolt.conf'],
  install_dir : join_paths(sysconfdir, 'dbus-1', 'system.d')
)

service_file = configure_file(
  input: 'data/org.freedesktop.bolt.service.in',
  output: 'org.freedesktop.bolt.service',
  configuration: subs
)

install_data(service_file,
  install_dir: join_paths(datadir, 'dbus-1', 'system-services')
)

policy_rules = configure_file(
  input: 'policy/org.freedesktop.bolt.rules.in',
  output: 'org.freedesktop.bolt.rules',
  configuration: subs
)

install_data(policy_rules,
  install_dir: join_paths(datadir, 'polkit-1', 'rules.d')
)

policy_file = configure_file(
  input: 'policy/org.freedesktop.bolt.policy.in',
  output: 'org.freedesktop.bolt.policy',
  configuration: subs
)

install_data(policy_file,
  install_dir: join_paths(datadir, 'polkit-1', 'actions')
)

unit_file = configure_file(
  input: 'data/bolt.service.in',
  output: 'bolt.service',
  configuration: subs
)

install_data(unit_file,
  install_dir: unitdir
)

install_data('data/90-bolt.rules',
  install_dir: join_paths(udevdir, 'rules.d')
)

daemon_deps = [
  glib,
  gio,
  libudev,
  polkit,
  unix,
  common
]

daemon_library = static_library('daemon',
  c_args : [cargs],
  sources: daemon_sources,
	dependencies: daemon_deps,
	include_directories: [
    include_directories('boltd')
])

libdaemon = declare_dependency(
  dependencies: daemon_deps,
	link_with: [daemon_library],
	include_directories: [
    include_directories('boltd')
])


executable('boltd',
  ['boltd/bolt-daemon.c'],
  dependencies: [libdaemon],
  c_args : [
    cargs,
  ],
  install: true,
  install_dir: libexecdir)


# command line tools

executable('boltctl',
   ['cli/bolt-client.c',
    'cli/bolt-device.c',
    'cli/bolt-proxy.c',
    'cli/boltctl.c'],
  dependencies: [glib,
                 gio,
                 unix,
                 common],
  c_args : [
    cargs,
  ],
  install: true,
  install_dir: bindir)


# testing

tests = [
  ['test-common'],
  ['test-store', [libdaemon]]
]

foreach t: tests
  test_name = t.get(0)
  test_deps = [common] + t.get(1, [])
  test_srcs = ['tests/@0@.c'.format(test_name), t.get(2, [])]
  e = executable(test_name, test_srcs, dependencies: test_deps)

  test_env = environment()
  test_env.prepend('LD_PRELOAD', 'libumockdev-preload.so.0')
  test(test_name, e, env: test_env, timeout : 120)

endforeach

# contrib

spec_file = configure_file(
  input: 'contrib/bolt.spec.in',
  output: 'bolt.spec',
  configuration: subs
)

# documentation

build_man = build_man != false and a2x.found()
if build_man

  man = custom_target(
    'boltctl-man',
    build_by_default: true,
    input: 'docs/boltctl.1.txt',
    output: 'boltctl.1',
    command: [
      a2x,
      '-d', 'manpage',
      '-f', 'manpage',
      '-D', '@OUTDIR@',
      '-a', 'manmanual=bolt Manual',
      '-a', 'mansource=bolt',
      '-a', 'revnumber=@0@'.format(meson.project_version()),
      '@INPUT@'
    ],
    install: true,
    install_dir: join_paths(mandir, 'man1'),
  )

endif

# all done, phew

msg = ['',
  'version:             @0@'.format(meson.project_version()),
  'database path:       @0@'.format(dbdir),
  'privileged group:    @0@'.format(privileged_group),
  '',
  'udevdir:             @0@'.format(udevdir),
  'systemd unitdir:     @0@'.format(unitdir),
  '',
  'build manpage:       @0@'.format(build_man),
  ''
  ]
message('\n  '.join(msg))
