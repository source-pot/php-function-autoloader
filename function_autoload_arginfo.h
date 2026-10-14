/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 1fa731fb8c6d3b9dbf55379ada7dbde5f1d172df */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_function_autoload_register, 0, 1, IS_TRUE, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, prepend, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_function_autoload_unregister, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_function_autoload_functions, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_function_autoload_call, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, function, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(function_autoload_register);
ZEND_FUNCTION(function_autoload_unregister);
ZEND_FUNCTION(function_autoload_functions);
ZEND_FUNCTION(function_autoload_call);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(function_autoload_register, arginfo_function_autoload_register)
	ZEND_FE(function_autoload_unregister, arginfo_function_autoload_unregister)
	ZEND_FE(function_autoload_functions, arginfo_function_autoload_functions)
	ZEND_FE(function_autoload_call, arginfo_function_autoload_call)
	ZEND_FE_END
};
