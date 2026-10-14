PHP_ARG_ENABLE([function_autoload],
  [whether to enable function autoloading support],
  [AS_HELP_STRING([--enable-function-autoload],
    [Enable function autoloading support])],
  [no])

if test "$PHP_FUNCTION_AUTOLOAD" != "no"; then
  AC_DEFINE(HAVE_FUNCTION_AUTOLOAD, 1, [Whether function autoloading is enabled])
  PHP_NEW_EXTENSION(function_autoload, function_autoload.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
