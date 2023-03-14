from os import system
import platform

if platform.system() == "Linux":
    print("OS: Linux")
    print("Packing assets into header file")

    system("asar pack assets assets.asar")
    system("xxd -i assets.asar > assets.h")
    system("cp assets.h build/assets.h")
    system("cp assets.asar build/assets.asar")
    system("rm assets.asar assets.h")
