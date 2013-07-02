

env = Environment(CCFLAGS='-g3 -Wall -Wextra -DDEBUG')


env.Library (target='lib/bstrlib',source='src/bstrlib');

libs = Split("""
     bstrlib
     nanomsg
     pthread
     m
     dl
""")

env.Program (target='bin/userd',source='src/userd.c',LIBS=libs,LIBPATH='lib');
env.Program (target='bin/userd-test',source='src/userd_test.c',LIBS=libs,LIBPATH='lib');

libs = Split("""
     bstrlib
     m
     dl
""")

env.SharedLibrary (target='bin/userd_null',source="src/userd_null.c",LIBS=libs,LIBPATH='lib');
env.SharedLibrary (target='bin/userd_array',source="src/userd_array.c",LIBS=libs,LIBPATH='lib');


