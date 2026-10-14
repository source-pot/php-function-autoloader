# function_autoload

A PHP extension that adds autoloading for **functions**, mirroring what
`spl_autoload_register()` does for classes.

```php
// app/Helpers/today.php
namespace App\Helpers;

function today(): string {
    return 'today';
}

// bootstrap
function_autoload_register(function (string $function): void {
    $file = __DIR__ . '/' . str_replace(['App\\', '\\'], ['app/', '/'], $function) . '.php';
    if (is_file($file)) {
        require $file;
    }
});

// anywhere else
namespace App\Controllers;
use function App\Helpers\today;

today(); // autoloaded on first call
```

## API

| Function | Description |
|---|---|
| `function_autoload_register(callable $callback, bool $prepend = false): true` | Add a loader. Duplicates are ignored. |
| `function_autoload_unregister(callable $callback): bool` | Remove a loader. |
| `function_autoload_functions(): array` | List registered loaders. |
| `function_autoload_call(string $function): bool` | Run the loaders for a name. Returns whether the function is defined afterwards. |

Loaders receive the fully qualified name without a leading backslash, in the
case it was written at the call site.

## What triggers autoloading

- `use function` imports and fully qualified calls (`\App\Helpers\today()`)
- Unqualified calls inside a namespace, **only if neither the namespaced nor
  the global function exists**. `strlen()` in a namespace never reaches a
  loader. When both are missing, the namespaced name is tried first, then the
  global one.
- Dynamic calls: `$fn = 'App\Helpers\today'; $fn();`
- `call_user_func()`, `call_user_func_array()`, `function_exists()` and
  `is_callable()` with a string name

Each call site resolves once. After that the engine's runtime cache is used,
so steady-state overhead is a single cache check.

## Known limitations

- **The OPcache JIT is disabled** while this extension is loaded. PHP turns
  off the JIT for any extension that installs user opcode handlers and emits a
  warning at startup. OPcache itself still works.
- Callables passed as strings to other internal functions (`array_map`,
  `usort`, `register_shutdown_function`, ...) are not autoloaded, because the
  engine resolves them without any hook point. Load them first with
  `function_autoload_call()`, or call them through a closure.
- Only tested on PHP 8.5 (NTS). Opcode operand layouts can change between
  PHP versions.

## Building

```bash
phpize
./configure --enable-function-autoload
make
make test
```

Then add `extension=function_autoload.so` to your `php.ini`, or try it with
`php -d extension=modules/function_autoload.so`.

## How it works

PHP compiles a call to a not-yet-known function into one of
`ZEND_INIT_FCALL_BY_NAME`, `ZEND_INIT_NS_FCALL_BY_NAME`,
`ZEND_INIT_DYNAMIC_CALL` or `ZEND_INIT_USER_CALL`. The extension registers
user opcode handlers for these. If the call site has not been resolved yet
and the function is missing, it runs the loaders and then hands control back
to the engine's own handler. The four internal functions listed above are
wrapped so they autoload before running.
