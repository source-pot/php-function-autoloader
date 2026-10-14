--TEST--
Loader exceptions propagate, unresolved calls still fail, recursion is guarded
--EXTENSIONS--
function_autoload
--FILE--
<?php
function_autoload_register(function (string $name) {
    echo "[loader] $name\n";
    if ($name === 'explodes') {
        throw new RuntimeException("cannot load $name");
    }
    if ($name === 'recursive') {
        // Asking for the same function while loading it must not loop.
        var_dump(function_exists('recursive'));
    }
});

try {
    explodes();
} catch (RuntimeException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

try {
    not_defined_anywhere();
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

try {
    recursive();
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
[loader] explodes
RuntimeException: cannot load explodes
[loader] not_defined_anywhere
Error: Call to undefined function not_defined_anywhere()
[loader] recursive
bool(false)
Error: Call to undefined function recursive()
