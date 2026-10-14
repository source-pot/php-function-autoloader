#ifndef PHP_FUNCTION_AUTOLOAD_H
#define PHP_FUNCTION_AUTOLOAD_H

extern zend_module_entry function_autoload_module_entry;
#define phpext_function_autoload_ptr &function_autoload_module_entry

#define PHP_FUNCTION_AUTOLOAD_VERSION "0.1.0"

ZEND_BEGIN_MODULE_GLOBALS(function_autoload)
	HashTable *loaders;  /* registered loader callables, in call order */
	HashTable *loading;  /* lowercased names currently being autoloaded */
ZEND_END_MODULE_GLOBALS(function_autoload)

ZEND_EXTERN_MODULE_GLOBALS(function_autoload)

#define FAG(v) ZEND_MODULE_GLOBALS_ACCESSOR(function_autoload, v)

#if defined(ZTS) && defined(COMPILE_DL_FUNCTION_AUTOLOAD)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
