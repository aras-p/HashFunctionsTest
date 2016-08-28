import os
import subprocess

apitarget = "android-23"

if subprocess.call("android update project -p . -t %s" % apitarget, shell=True) == 0:
    print("Project updated to %s" % apitarget)
else:
    print("Error! Could not update project. Android NDK installed?")
