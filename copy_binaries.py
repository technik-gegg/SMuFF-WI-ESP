Import( 'env' ) # type: ignore

import os.path
import shutil

build_flags     = env.ParseFlags(env["BUILD_FLAGS"]) # type: ignore
env_defines     = build_flags.get("CPPDEFINES")
project_dir     = env["PROJECT_DIR"] # type: ignore
pioenv          = env["PIOENV"] # type: ignore
version         = 'v0.0.0'

for define in env_defines:
    if define[0] == "VERSION":
        version = 'v'+define[1].replace("\"", "")

def copy_file(source, target, env):
    source = target[0].get_abspath()
    #print("Source: " + source)
    source_file = os.path.basename(source)
    #print("File: "+ source_file)
    target_dir = os.path.join(project_dir, 'latest_binaries', pioenv, version)
    #print("Target: "+ target_dir)

    if os.path.exists(target_dir) == False:
        os.makedirs(target_dir)

    try:
        shutil.copy(source, os.path.join(target_dir, source_file))
        print("'{0}' successfully copied to latest_binaries folder.".format(source_file))
    except FileNotFoundError:
        print("'{0}' doesn't exist, skipping.".format(source_file))

env.AddPostAction( '$BUILD_DIR/firmware.bin', copy_file) # type: ignore
env.AddPostAction( '$BUILD_DIR/littlefs.bin', copy_file) # type: ignore