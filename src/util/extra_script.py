Import("env", "projenv")
import subprocess

def get_revisions(env, projenv):
  git_rev = (
    subprocess.check_output(["git", "rev-parse", "HEAD"])
    .strip()
    .decode("utf-8")
  )
  
  
  isGitDirty = False
  try:
    gitDirtyOut = subprocess.check_output(["git", "diff-index", "--quiet", "HEAD"])
  except subprocess.CalledProcessError as gitDirtyOut:
    isGitDirty = True
    print("WARNING:  Git has uncommitted changes")
  
  print("GIT_REV=%6.6s" % git_rev)
  projenv.Append(CPPDEFINES=[("GIT_REV", '\\"%6.6s\\"' % git_rev)])

  major = int(env.GetProjectOption("custom_major_version"))
  minor = int(env.GetProjectOption("custom_minor_version"))
  delta = int(env.GetProjectOption("custom_delta_version"))
  print("MAJOR=%d" % major)
  print("MINOR=%d" % minor)
  print("DELTA=%d" % delta)


  projenv.Append(CPPDEFINES=[("MAJOR_VERSION", "%d" % major)])
  projenv.Append(CPPDEFINES=[("MINOR_VERSION", "%d" % minor)])
  projenv.Append(CPPDEFINES=[("DELTA_VERSION", "%d" % minor)])

  revstr = "%d_%d_%d-%6.6s" % (major, minor, delta, git_rev)
  if isGitDirty:
    revstr = revstr + "-unclean"

  return revstr

revstr = get_revisions(env, projenv)

env.AddPostAction(
  "$BUILD_DIR/${PROGNAME}.bin",
  env.VerboseAction(" ".join([
    "python", "src/util/join_bins.py", "$BUILD_DIR/mrbwwifi-%s-master.bin" % (revstr), "0x1000", "src-platformio/arduino/ise_mrbwwifi_esp32s2/bootloader-tinyuf2.bin", "0x8000", 
    "$BUILD_DIR/partitions.bin", "0xE000", "src-platformio/arduino/ise_mrbwwifi_esp32s2/boot_app0.bin", "0x10000", "$BUILD_DIR/${PROGNAME}.bin", 
    "0x2d0000", "src-platformio/arduino/ise_mrbwwifi_esp32s2/tinyuf2.bin"]), "Building master firmware image mrbwwifi-%s-master.bin" % (revstr))
)

env.AddPostAction(
  "$BUILD_DIR/${PROGNAME}.bin",
  env.VerboseAction(" ".join([
    "python", "src/util/uf2conv.py", "-f", "0xbfdd4eee", "-b", "0",
    "-c", "-o" "$BUILD_DIR/mrbwwifi-%s.uf2" % (revstr), "$BUILD_DIR/${PROGNAME}.bin"]), "Building mrbwwifi-%s.uf2" % (revstr))
)
