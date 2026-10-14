--TEST--
Unqualified calls in a namespace: existing global functions never hit the loader
--EXTENSIONS--
function_autoload
--FILE--
<?php
namespace App\Controllers;

require __DIR__ . '/loader.inc';

// Global function exists, so no autoloading happens.
var_dump(strlen('abc'));

// Neither App\Controllers\local nor \local exist: the namespaced name is tried first.
var_dump(local());

// Namespaced name is not found, so the global name is tried next.
var_dump(global_helper());
?>
--EXPECT--
int(3)
[loader] App\Controllers\local
string(16) "namespaced local"
[loader] App\Controllers\global_helper
[loader] global_helper
string(13) "global helper"
