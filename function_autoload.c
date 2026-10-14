#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "php_function_autoload.h"
#include "function_autoload_arginfo.h"

ZEND_DECLARE_MODULE_GLOBALS(function_autoload)

/*
 * Opcodes that look a function up by name at runtime. Calls to functions
 * already known at compile time use ZEND_INIT_FCALL and never miss.
 */
static const uint8_t hooked_opcodes[] = {
	ZEND_INIT_FCALL_BY_NAME,    /* today() via `use function`, or \App\today() */
	ZEND_INIT_NS_FCALL_BY_NAME, /* today() inside a namespace, with global fallback */
	ZEND_INIT_DYNAMIC_CALL,     /* $fn() where $fn is a string */
	ZEND_INIT_USER_CALL,        /* call_user_func() compiled to an opcode */
};

static user_opcode_handler_t previous_opcode_handlers[256];

/*
 * Internal functions that take a function name as their first argument and
 * look it up without going through any of the opcodes above.
 */
typedef struct {
	const char *name;
	zend_internal_function *func;
	zif_handler original;
} wrapped_function;

static wrapped_function wrapped_functions[] = {
	{ "function_exists" },
	{ "is_callable" },
	{ "call_user_func" },
	{ "call_user_func_array" },
};

/* ---------------------------------------------------------------------- */
/* Loader registry                                                         */
/* ---------------------------------------------------------------------- */

static HashTable *loaders(void)
{
	if (!FAG(loaders)) {
		ALLOC_HASHTABLE(FAG(loaders));
		zend_hash_init(FAG(loaders), 4, NULL, ZVAL_PTR_DTOR, 0);
	}
	return FAG(loaders);
}

static zend_long find_loader(zval *callback)
{
	zend_ulong index;
	zval *loader;

	ZEND_HASH_FOREACH_NUM_KEY_VAL(loaders(), index, loader) {
		if (zend_is_identical(loader, callback)) {
			return (zend_long) index;
		}
	} ZEND_HASH_FOREACH_END();

	return -1;
}

static void free_table(HashTable **table)
{
	if (*table) {
		zend_hash_destroy(*table);
		FREE_HASHTABLE(*table);
		*table = NULL;
	}
}

/* ---------------------------------------------------------------------- */
/* Autoloading                                                             */
/* ---------------------------------------------------------------------- */

static bool function_is_defined(zend_string *lc_name)
{
	return zend_hash_exists(EG(function_table), lc_name);
}

static bool is_loading(zend_string *lc_name)
{
	return FAG(loading) && zend_hash_exists(FAG(loading), lc_name);
}

static void mark_loading(zend_string *lc_name)
{
	if (!FAG(loading)) {
		ALLOC_HASHTABLE(FAG(loading));
		zend_hash_init(FAG(loading), 4, NULL, NULL, 0);
	}
	zend_hash_add_empty_element(FAG(loading), lc_name);
}

static void unmark_loading(zend_string *lc_name)
{
	zend_hash_del(FAG(loading), lc_name);
}

/*
 * Calls each loader in turn until one of them defines `name` (which must not
 * have a leading backslash). Iterates over a copy so loaders can register or
 * unregister loaders while running.
 */
static void run_loaders(zend_string *name, zend_string *lc_name)
{
	HashTable *snapshot = zend_array_dup(loaders());
	zval arg, retval, *loader;

	ZVAL_STR(&arg, name);

	ZEND_HASH_FOREACH_VAL(snapshot, loader) {
		if (call_user_function(NULL, NULL, loader, &retval, 1, &arg) == SUCCESS) {
			zval_ptr_dtor(&retval);
		}
		if (EG(exception) || function_is_defined(lc_name)) {
			break;
		}
	} ZEND_HASH_FOREACH_END();

	zend_array_destroy(snapshot);
}

/*
 * Ensures the function `name` is defined, autoloading it if needed.
 * Returns whether it is defined afterwards. Names that can never be plain
 * functions ("Foo::bar", "") are ignored.
 */
static bool autoload_function(zend_string *name)
{
	const char *str = ZSTR_VAL(name);
	size_t len = ZSTR_LEN(name);

	if (len > 0 && str[0] == '\\') {
		str++;
		len--;
	}
	if (len == 0 || memchr(str, ':', len)) {
		return false;
	}

	zend_string *bare = zend_string_init(str, len, 0);
	zend_string *lc_name = zend_string_tolower(bare);
	bool defined = function_is_defined(lc_name);

	if (!defined && FAG(loaders) && !is_loading(lc_name)) {
		mark_loading(lc_name);
		run_loaders(bare, lc_name);
		unmark_loading(lc_name);
		defined = function_is_defined(lc_name);
	}

	zend_string_release(lc_name);
	zend_string_release(bare);
	return defined;
}

static void autoload_if_string(zval *value)
{
	ZVAL_DEREF(value);
	if (Z_TYPE_P(value) == IS_STRING) {
		autoload_function(Z_STR_P(value));
	}
}

/* For App\Controllers\today, returns "today". */
static zend_string *unqualified_name(zend_string *qualified)
{
	const char *slash = zend_memrchr(ZSTR_VAL(qualified), '\\', ZSTR_LEN(qualified));
	const char *start = slash ? slash + 1 : ZSTR_VAL(qualified);

	return zend_string_init(start, ZSTR_VAL(qualified) + ZSTR_LEN(qualified) - start, 0);
}

/* ---------------------------------------------------------------------- */
/* Opcode hooks                                                            */
/* ---------------------------------------------------------------------- */

/* Rethrows a loader's exception, otherwise hands over to the real handler. */
static int finish_hook(zend_execute_data *execute_data)
{
	if (EG(exception)) {
		zend_rethrow_exception(execute_data);
		return ZEND_USER_OPCODE_CONTINUE;
	}

	user_opcode_handler_t previous = previous_opcode_handlers[EX(opline)->opcode];
	return previous ? previous(execute_data) : ZEND_USER_OPCODE_DISPATCH;
}

/*
 * Operand layout comes from zend_compile.c:
 *   INIT_FCALL_BY_NAME     op2 = [name, lc_name]
 *   INIT_NS_FCALL_BY_NAME  op2 = [ns_name, lc_ns_name, lc_global_name]
 * The cache slot is filled once a call site has resolved its function.
 */
static int on_init_fcall_by_name(zend_execute_data *execute_data)
{
	const zend_op *opline = EX(opline);
	zval *names = RT_CONSTANT(opline, opline->op2);

	if (!CACHED_PTR(opline->result.num) && !function_is_defined(Z_STR(names[1]))) {
		autoload_function(Z_STR(names[0]));
	}

	return finish_hook(execute_data);
}

/*
 * Unqualified call inside a namespace. Existing functions always win, so
 * strlen() in a namespace never reaches a loader. Only when neither the
 * namespaced nor the global function exists do we autoload: namespaced name
 * first, then the global one.
 */
static int on_init_ns_fcall_by_name(zend_execute_data *execute_data)
{
	const zend_op *opline = EX(opline);
	zval *names = RT_CONSTANT(opline, opline->op2);

	if (CACHED_PTR(opline->result.num)
			|| function_is_defined(Z_STR(names[1]))
			|| function_is_defined(Z_STR(names[2]))) {
		return finish_hook(execute_data);
	}

	if (!autoload_function(Z_STR(names[0])) && !EG(exception)) {
		zend_string *global_name = unqualified_name(Z_STR(names[0]));
		autoload_function(global_name);
		zend_string_release(global_name);
	}

	return finish_hook(execute_data);
}

/* $fn() and call_user_func(): the callable is in op2, which may be a CONST, TMP or CV. */
static int on_init_call_by_operand(zend_execute_data *execute_data)
{
	const zend_op *opline = EX(opline);
	zval *callable = opline->op2_type == IS_CONST
		? RT_CONSTANT(opline, opline->op2)
		: EX_VAR(opline->op2.var);

	autoload_if_string(callable);

	return finish_hook(execute_data);
}

static user_opcode_handler_t hook_for(uint8_t opcode)
{
	switch (opcode) {
		case ZEND_INIT_FCALL_BY_NAME:    return on_init_fcall_by_name;
		case ZEND_INIT_NS_FCALL_BY_NAME: return on_init_ns_fcall_by_name;
		default:                         return on_init_call_by_operand;
	}
}

static void install_opcode_hooks(void)
{
	for (size_t i = 0; i < sizeof(hooked_opcodes); i++) {
		uint8_t opcode = hooked_opcodes[i];
		previous_opcode_handlers[opcode] = zend_get_user_opcode_handler(opcode);
		zend_set_user_opcode_handler(opcode, hook_for(opcode));
	}
}

static void remove_opcode_hooks(void)
{
	for (size_t i = 0; i < sizeof(hooked_opcodes); i++) {
		uint8_t opcode = hooked_opcodes[i];
		zend_set_user_opcode_handler(opcode, previous_opcode_handlers[opcode]);
	}
}

/* ---------------------------------------------------------------------- */
/* Wrapped internal functions                                              */
/* ---------------------------------------------------------------------- */

static wrapped_function *find_wrapped(zend_function *func)
{
	for (size_t i = 0; i < sizeof(wrapped_functions) / sizeof(wrapped_functions[0]); i++) {
		if (wrapped_functions[i].func == &func->internal_function) {
			return &wrapped_functions[i];
		}
	}
	return NULL;
}

static ZEND_NAMED_FUNCTION(autoloading_wrapper)
{
	wrapped_function *wrapped = find_wrapped(EX(func));

	if (ZEND_NUM_ARGS() > 0) {
		autoload_if_string(ZEND_CALL_ARG(execute_data, 1));
		if (EG(exception)) {
			RETURN_THROWS();
		}
	}

	wrapped->original(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static void install_function_wrappers(void)
{
	for (size_t i = 0; i < sizeof(wrapped_functions) / sizeof(wrapped_functions[0]); i++) {
		wrapped_function *wrapped = &wrapped_functions[i];
		zend_function *func = zend_hash_str_find_ptr(CG(function_table), wrapped->name, strlen(wrapped->name));

		if (func && func->type == ZEND_INTERNAL_FUNCTION) {
			wrapped->func = &func->internal_function;
			wrapped->original = func->internal_function.handler;
			func->internal_function.handler = autoloading_wrapper;
		}
	}
}

static void remove_function_wrappers(void)
{
	for (size_t i = 0; i < sizeof(wrapped_functions) / sizeof(wrapped_functions[0]); i++) {
		wrapped_function *wrapped = &wrapped_functions[i];

		if (wrapped->func) {
			wrapped->func->handler = wrapped->original;
			wrapped->func = NULL;
		}
	}
}

/* ---------------------------------------------------------------------- */
/* Userland API                                                            */
/* ---------------------------------------------------------------------- */

PHP_FUNCTION(function_autoload_register)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	bool prepend = false;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_FUNC(fci, fcc)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(prepend)
	ZEND_PARSE_PARAMETERS_END();

	zval *callback = &fci.function_name;

	if (find_loader(callback) >= 0) {
		RETURN_TRUE;
	}

	Z_TRY_ADDREF_P(callback);

	if (!prepend) {
		zend_hash_next_index_insert(loaders(), callback);
		RETURN_TRUE;
	}

	HashTable *reordered = zend_new_array(zend_hash_num_elements(loaders()) + 1);
	zval *loader;

	zend_hash_next_index_insert(reordered, callback);
	ZEND_HASH_FOREACH_VAL(loaders(), loader) {
		Z_TRY_ADDREF_P(loader);
		zend_hash_next_index_insert(reordered, loader);
	} ZEND_HASH_FOREACH_END();

	free_table(&FAG(loaders));
	FAG(loaders) = reordered;
	RETURN_TRUE;
}

PHP_FUNCTION(function_autoload_unregister)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_FUNC(fci, fcc)
	ZEND_PARSE_PARAMETERS_END();

	zend_long index = find_loader(&fci.function_name);

	if (index < 0) {
		RETURN_FALSE;
	}

	zend_hash_index_del(loaders(), index);
	RETURN_TRUE;
}

PHP_FUNCTION(function_autoload_functions)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	zval *loader;
	ZEND_HASH_FOREACH_VAL(loaders(), loader) {
		Z_TRY_ADDREF_P(loader);
		add_next_index_zval(return_value, loader);
	} ZEND_HASH_FOREACH_END();
}

PHP_FUNCTION(function_autoload_call)
{
	zend_string *name;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(name)
	ZEND_PARSE_PARAMETERS_END();

	bool defined = autoload_function(name);

	if (EG(exception)) {
		RETURN_THROWS();
	}
	RETURN_BOOL(defined);
}

/* ---------------------------------------------------------------------- */
/* Module lifecycle                                                        */
/* ---------------------------------------------------------------------- */

static PHP_GINIT_FUNCTION(function_autoload)
{
#if defined(COMPILE_DL_FUNCTION_AUTOLOAD) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	function_autoload_globals->loaders = NULL;
	function_autoload_globals->loading = NULL;
}

PHP_MINIT_FUNCTION(function_autoload)
{
	install_opcode_hooks();
	install_function_wrappers();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(function_autoload)
{
	remove_function_wrappers();
	remove_opcode_hooks();
	return SUCCESS;
}

PHP_RINIT_FUNCTION(function_autoload)
{
#if defined(ZTS) && defined(COMPILE_DL_FUNCTION_AUTOLOAD)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(function_autoload)
{
	free_table(&FAG(loaders));
	free_table(&FAG(loading));
	return SUCCESS;
}

PHP_MINFO_FUNCTION(function_autoload)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "function autoloading", "enabled");
	php_info_print_table_row(2, "version", PHP_FUNCTION_AUTOLOAD_VERSION);
	php_info_print_table_end();
}

zend_module_entry function_autoload_module_entry = {
	STANDARD_MODULE_HEADER,
	"function_autoload",
	ext_functions,
	PHP_MINIT(function_autoload),
	PHP_MSHUTDOWN(function_autoload),
	PHP_RINIT(function_autoload),
	PHP_RSHUTDOWN(function_autoload),
	PHP_MINFO(function_autoload),
	PHP_FUNCTION_AUTOLOAD_VERSION,
	PHP_MODULE_GLOBALS(function_autoload),
	PHP_GINIT(function_autoload),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_FUNCTION_AUTOLOAD
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(function_autoload)
#endif
