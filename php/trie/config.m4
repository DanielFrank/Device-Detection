PHP_ARG_ENABLE(fiftyone_degrees_detector, whether to enable 51Degrees Detector,
[ --enable-fiftyone_degrees_detector   Enable 51Degrees Device Detection])

if test "$PHP_FIFTYONE_DEGREES_DETECTOR" = "yes"; then
  AC_CONFIG_COMMANDS_PRE(mkdir src/trie)
  AC_CONFIG_COMMANDS_PRE(mkdir src/cityhash)
  AC_CONFIG_COMMANDS_PRE(cp ../../src/trie/51Degrees.* src/trie)
  AC_CONFIG_COMMANDS_PRE(cp ../../src/cityhash/city.* src/cityhash)

  AC_DEFINE(HAVE_FIFTYONE_DEGREES_DETECTOR, 1, [Whether you have 51Degrees Detector Enabled])
  PHP_SUBST(FIFTYONE_DEGREES_DETECTOR_LIBADD)
  PHP_NEW_EXTENSION(fiftyone_degrees_detector, src/cityhash/city.c src/trie/51Degrees.c src/fiftyone_degrees_v3_extension.c, $ext_shared,, -D HAVE_SNPRINTF)
fi

