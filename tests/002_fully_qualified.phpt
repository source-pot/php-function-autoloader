--TEST--
Fully qualified call is autoloaded, and each call site resolves only once
--EXTENSIONS--
function_autoload
--FILE--
<?php
require __DIR__ . '/loader.inc';

for ($i = 0; $i < 3; $i++) {
    echo \App\Helpers\shout('hi'), "\n";
}
echo App\Helpers\shout('again'), "\n";
?>
--EXPECT--
[loader] App\Helpers\shout
HI!
HI!
HI!
AGAIN!
