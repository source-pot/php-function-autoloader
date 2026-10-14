--TEST--
Dynamic calls, call_user_func(), function_exists() and is_callable() autoload
--EXTENSIONS--
function_autoload
--FILE--
<?php
require __DIR__ . '/loader.inc';

$fn = 'App\Helpers\shout';
var_dump($fn('dynamic'));

var_dump(\call_user_func('\App\Helpers\today'));

function_autoload_unregister(function_autoload_functions()[0]);
function_autoload_register(function (string $name) {
    echo "[loader] $name\n";
    eval("function $name() { return '$name'; }");
});

var_dump(function_exists('via_function_exists'));
var_dump(is_callable('via_is_callable'));
var_dump(call_user_func_array('via_call_user_func_array', []));
$name = 'via_call_user_func';
var_dump(call_user_func($name));

// Static method strings are left alone.
var_dump(is_callable('Nope::method'));
?>
--EXPECT--
[loader] App\Helpers\shout
string(8) "DYNAMIC!"
[loader] App\Helpers\today
string(5) "today"
[loader] via_function_exists
bool(true)
[loader] via_is_callable
bool(true)
[loader] via_call_user_func_array
string(24) "via_call_user_func_array"
[loader] via_call_user_func
string(18) "via_call_user_func"
bool(false)
