<?php
// Maps App\Helpers\today to fixtures/App/Helpers/today.php
function_autoload_register(function (string $name): void {
    echo "[loader] $name\n";
    $file = __DIR__ . '/fixtures/' . str_replace('\\', '/', $name) . '.php';
    if (is_file($file)) {
        require $file;
    }
});
