#!python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPDEFINES=["NGTCP2_STATICLIB"])

env.Append(CPPPATH=[
    "thirdparty/boringssl/include",
    "thirdparty/ngtcp2/lib/includes",
    "thirdparty/ngtcp2/crypto/includes",
    "thirdparty/ngtcp2/build_win/lib/includes",
    "thirdparty/ngtcp2/build_linux/lib/includes"
])


# Branchement conditionnel selon la plateforme d'export
if env["platform"] == "windows":
    env.Append(LIBPATH=[
        "thirdparty/boringssl/build_win/Release",
        "thirdparty/ngtcp2/build_win/lib/Release",
        "thirdparty/ngtcp2/build_win/crypto/boringssl/Release"
    ])
    env.Append(LIBS=[
        "ngtcp2_crypto_boringssl",
        "ngtcp2",
        "ssl",
        "crypto",
        "ws2_32",
        "advapi32",
        "crypt32"
    ])
elif env["platform"] in ["linux", "linuxbsd"]:
    env.Append(LIBPATH=[
        "thirdparty/boringssl/build_linux",               # Correspond à ton précédent 'ls'
        "thirdparty/ngtcp2/build_linux/lib",              # Emplacement standard de libngtcp2.a
        "thirdparty/ngtcp2/build_linux/crypto/boringssl"  # Emplacement standard du pont crypto
    ])
    env.Append(LIBS=[
        "ngtcp2_crypto_boringssl",
        "ngtcp2",
        "ssl",
        "crypto",
        "pthread" # Requis sous Linux pour les opérations multithreadées de BoringSSL
    ])

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp") + Glob("src/*/*.cpp")

# Compilation de la bibliothèque partagée (gère automatiquement l'extension .so ou .dll)
library = env.SharedLibrary(
    "demo/bin/libnetwork_RAN{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
    source=sources,
)
Default(library)