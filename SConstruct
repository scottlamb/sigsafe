# vim: ft=python
# $Id$
# Copyright (C) 2004 Scott Lamb <slamb@slamb.org>.
# This file is part of sigsafe, which is released under the MIT license.

import os
import re
import string

debug = 1

#
# Determine our platform
#
# We use this to pick what assembly to use and for some tests below.
#

arch = os.uname()[4]
if arch == 'Power Macintosh': arch = 'ppc'
if re.compile('i[3456]86').match(arch): arch = 'i386'
if re.compile('sun.*').match(arch): arch = 'sparc'
os_name = string.replace(string.lower(os.uname()[0]),'-','')
Export('arch os_name')

#
# Build a basic environment
#
# This is shared between the single-threaded and multi-threaded builds, hence
# the name "global_env".
#

global_env = None
if os_name == 'osf1':
    # gcc doesn't work with pthreads, so force use of the native tools
    global_env = Environment(tools = ['cc','link','ar','as'])
else:
    global_env = Environment()

global_env.Append(
    CPPPATH = [
        '#/src',
        '#/src/' + arch + '-' + os_name
    ],
)

#
# Modify the basic environment according to our platform.
#
# There's a mix here of behavior hardcoded for each platform and behavior that
# is tested for. Stuff generally gets tested for if it is very simple or may
# change across revisions of the operating system. Otherwise, we can get away
# with tests by platform name because we need a specific effort to port to
# each system anyway - to write the assembly code.
#

if os_name == 'darwin':
    global_env['SHLINKFLAGS'] = '$LINKFLAGS -dynamiclib -Wl,-x'
    global_env['SHLIBSUFFIX'] = '.dylib'

if os_name == 'osf1':
    global_env.Append(CPPDEFINES = [
        '_XOPEN_SOURCE=600',     # use socklen_t
        '_OSF_SOURCE',           # ... but publish mcontext_t members anyway
    ])

if global_env['CC'] == 'gcc':
    global_env.Append(CCFLAGS = ['-Wall', '-fno-common'])

if debug:
    global_env.Append(
        CPPDEFINES=[
            'SIGSAFE_DEBUG_SIGNAL', # write [S] to stderr whenever
                                    # safe signal received
            'SIGSAFE_DEBUG_JUMP',   # write [J] to stderr whenever
                                    # jumping from sighandler
        ],
        CCFLAGS=['-g'],
        LINKFLAGS=['-g'],
    )

conf = global_env.Configure(conf_dir = '.sconf_temp_%s-%s' % (arch, os_name))

if os_name != 'darwin':
    # Darwin poll is emulated through select
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_POLL'])

if os_name != 'sunos':
    # Solaris select is emulated through poll
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_SELECT'])

if conf.CheckFunc('kevent'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_KEVENT'])

if conf.CheckFunc('epoll_wait'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_EPOLL'])

if conf.CheckHeader('stdint.h'):
    conf.env.Append(CPPDEFINES = ['SIGSAFE_HAVE_STDINT_H'])

# Check for stuff needed only for testing.

extra_test_libs = []

if not conf.CheckFunc('nanosleep'):
    if conf.CheckLibWithHeader('rt', 'time.h', 'C', 'nanosleep(NULL,NULL);',
                               autoadd = 0):
        extra_test_libs.append('rt')
    else:
        print 'Unable to find library for nanosleep.'
        Exit(1)

if not conf.CheckFunc('connect'):
    if conf.CheckLib('nsl', autoadd = 0):
        extra_test_libs.append('nsl')
    if conf.CheckLibWithHeader('socket', ['sys/types.h','sys/socket.h'],
                               'C', 'connect(0, NULL, 0);', autoadd = 0):
        extra_test_libs.append('socket')
    else:
        print 'Unable to find library for connect.'
        Exit(1)

Export('extra_test_libs')

global_env = conf.Finish()

#
# Now build single-threaded and multi-threaded variants.
#
# addTargets adds the targets for the given build type with the given
# environment.
#

def addTargets(type, base_env):
    buildDir = 'build-%s-%s-%s' % (arch, os_name, type)
    env = base_env.Copy()
    env.Append(LIBPATH = '#/' + buildDir)
    Export('env')

    SConscript('src/SConscript',
               build_dir = buildDir,
               duplicate = 0)
    SConscript('tests/SConscript',
               build_dir = buildDir + '/tests/',
               duplicate = 0)

# Always build a single-threaded version.
addTargets('st', global_env)

# Build a multi-threaded version if our platform has a good thread library.
if os_name != 'freebsd' and os_name != 'netbsd':
    env = global_env.Copy()
    env.Append(
        CPPDEFINES = [
            '_REENTRANT',
            '_THREAD_SAFE',
        ]
    )
    if os_name == 'freebsd':
        if True:
            # use LinuxThreads
            env.Append(
                CPPPATH = ['/usr/local/include/pthread/linuxthreads'],
                LIBPATH = ['/usr/local/lib'],
                LIBS = ['lthread'],
            )
        else:
            # use built-in pthreads
            # DON'T USE ON 4.x, as this is a user threading library.
            # I think 5.x has a real library with the same flag. Haven't tried.
            env.Append(
                CCFLAGS = ['-pthread'],
                LINKFLAGS = ['-pthread']
            )
    elif os_name == 'osf1':
        env.Append(
            CCFLAGS = ['-pthread'],
            LINKFLAGS = ['-pthread']
        )
    else:
        env.Append(LIBS = ['pthread'])
    addTargets('mt', env)
