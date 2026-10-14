<?php

// Maps App\Helpers\today to App/Helpers/today.php, next to this file.
function_autoload_register(function (string $function): void {
    $file = __DIR__ . '/' . str_replace('\\', '/', $function) . '.php';

    if (is_file($file)) {
        require $file;
    }
});
