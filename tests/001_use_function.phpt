--TEST--
Function imported with `use function` is autoloaded on first call
--EXTENSIONS--
function_autoload
--FILE--
<?php
namespace App\Controllers;

use function App\Helpers\today;

require __DIR__ . '/loader.inc';

var_dump(today());
var_dump(today());
?>
--EXPECT--
[loader] App\Helpers\today
string(5) "today"
string(5) "today"
