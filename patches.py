from os.path import join, isfile

Import("env")

try:
    import patch
except ImportError:
    env.Execute("$PYTHONEXE -m pip install patch")
    import patch

dnFramework = env.PioPlatform().get_package_dir("framework-arduinoteensy")
dnLib       = join(dnFramework, "libraries", "SD", "src")
fnPatchFlag = join(dnLib, ".patched")

# patch file only if we didn't do it before
if not isfile(fnPatchFlag):
    fnOriginal = join(dnLib, "SD.h")
    fnPatch    = join("patches", "SD.patch")

    assert isfile(fnOriginal) and isfile(fnPatch)
    env.Execute("patch %s %s" % (fnOriginal, fnPatch))

    def _touch(path):
        with open(path, "w") as fp:
            fp.write("")

    env.Execute(lambda *args, **kwargs: _touch(fnPatchFlag))
