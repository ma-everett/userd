

#env = Environment(CCFLAGS='-g3 -Wall -DDEBUG')
env = Environment()

src = Split("""
    src/userd_client.c
    src/bstrlib.c
""")

env.Library (target='lib/userd',source=src);

libs = Split("""
     userd
     m
     dl
""")

env.Program (target='bin/userd',source='src/userd.c',LIBS=libs,LIBPATH='lib');
env.Program (target='bin/userd-test',source='src/userd_test.c',LIBS=libs,LIBPATH='lib');

env.SharedLibrary (target='bin/userd_null',source="src/userd_null.c",LIBS=libs,LIBPATH='lib');
env.SharedLibrary (target='bin/userd_array',source="src/userd_array.c",LIBS=libs,LIBPATH='lib');


env.Program (target='bin/example',source='src/example.c',LIBS=libs,LIBPATH='lib');
