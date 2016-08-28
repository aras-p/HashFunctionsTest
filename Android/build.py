import os
import glob
import shutil
import subprocess
import sys

if not os.path.exists("assets/TestData"):
    os.makedirs("assets/TestData")

for file in glob.glob("../TestData/*.*"):
    shutil.copy(file, "assets/TestData")    

if subprocess.call("ndk-build", shell=True) == 0:
    print("Build successful")

    if subprocess.call("ant debug -Dout.final.file=hashtest.apk", shell=True) == 0:
        if sys.argv[1] == "-deploy":

            if subprocess.call("adb install -r hashtest.apk", shell=True) != 0:
                print("Could not deploy to device!")

    else:
        print("Error during build process!")

else:
    print("Error building project!")
