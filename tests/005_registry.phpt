--TEST--
Registering, prepending, unregistering and listing loaders
--EXTENSIONS--
function_autoload
--FILE--
<?php
$a = function (string $name) { echo "a: $name\n"; };
$b = function (string $name) { echo "b: $name\n"; };

var_dump(function_autoload_register($a));
var_dump(function_autoload_register($a)); // duplicate is ignored
function_autoload_register($b, prepend: true);
var_dump(count(function_autoload_functions()));

var_dump(function_autoload_call('missing_fn'));

var_dump(function_autoload_unregister($b));
var_dump(function_autoload_unregister($b));
var_dump(function_autoload_call('missing_fn'));

// Already defined functions never reach a loader.
var_dump(function_autoload_call('strlen'));

try {
    function_autoload_register('not_a_real_function');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
int(2)
b: missing_fn
a: missing_fn
bool(false)
bool(true)
bool(false)
a: missing_fn
bool(false)
bool(true)
function_autoload_register(): Argument #1 ($callback) must be a valid callback, function "not_a_real_function" not found or invalid function name
