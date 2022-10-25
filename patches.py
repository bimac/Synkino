from os.path import join, isfile
from shutil import which

Import("env")

# ensure that diff_match_patch is available
try:
    from diff_match_patch import diff_match_patch
except ImportError:
    env.Execute("$PYTHONEXE -m pip install diff_match_patch")
    from diff_match_patch import diff_match_patch

# create path and file names
dnFramework = env.PioPlatform().get_package_dir("framework-arduinoteensy")
dnLib       = join(dnFramework, "libraries", "SD", "src")
fnPatchFlag = join(dnLib, ".patched")

# patch file only if we didn't do it before
if not isfile(fnPatchFlag):

    # create file names & check if files exist
    fnOriginal = join(dnLib, "SD.h")
    fnPatch    = join("patches", "SD.patch")
    assert isfile(fnOriginal) and isfile(fnPatch)

    # read original file & patch
    dmp = diff_match_patch()
    with open(fnOriginal, "r") as fp:
        strOriginal = fp.read()
    with open(fnPatch, "r") as fp:
        patch = dmp.patch_fromText(fp.read())

    # apply patch
    print("Applying patch to %s" % fnOriginal)
    strPatched = dmp.patch_apply(patch,strOriginal)
    with open(fnOriginal, "w") as fp:
        fp.write(strPatched[0])

    # create patch flag
    def _touch(path):
        with open(path, "w") as fp:
            fp.write("")
    env.Execute(lambda *args, **kwargs: _touch(fnPatchFlag))
