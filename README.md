# function_autoload

A PHP extension that adds autoloading for **functions**, the same way
`spl_autoload_register()` works for classes.

PHP can lazily load classes, but not functions. If you put a helper function
in its own file and import it with `use function`, PHP simply fails with
`Call to undefined function` unless something has already included that file.
The usual workarounds are eagerly including every helper file (Composer's
`"files"` autoload) or wrapping functions as static methods on a class.

This extension removes the need for either. Register a loader, and the first
call to a missing function runs it.

## A complete example

The [`example/`](example/) directory is a three-file app. You can run it once
the extension is built (see [Installation](#installation)).

```
example/
├── index.php
├── autoload.php
└── App/
    └── Helpers/
        └── today.php
```

**`App/Helpers/today.php`** is a plain function in a namespace. Nothing
includes this file directly.

```php
<?php

namespace App\Helpers;

function today(): string {
    return 'Today is ' . date('l jS F Y');
}
```

**`autoload.php`** registers a loader. It receives the fully qualified
function name (`App\Helpers\today`), turns it into a path
(`App/Helpers/today.php`) and requires the file if it exists.

```php
<?php

// Maps App\Helpers\today to App/Helpers/today.php, next to this file.
function_autoload_register(function (string $function): void {
    $file = __DIR__ . '/' . str_replace('\\', '/', $function) . '.php';

    if (is_file($file)) {
        require $file;
    }
});
```

**`index.php`** is the entry point. It sets up autoloading, imports the
function and calls it.

```php
<?php

namespace App\Controllers;

require __DIR__ . '/autoload.php';

use function App\Helpers\today;

echo today(), "\n";
```

Run it:

```bash
php -d extension=modules/function_autoload.so example/index.php
```

```
Today is Wednesday 14th October 2026
```

On the first call to `today()`, PHP finds the function missing, the extension
calls your loader with `App\Helpers\today`, the loader requires the file, and
the call carries on as normal. Later calls go straight to the function.

## Installation

### Requirements

- PHP 8.5 (currently the only tested version) with its development headers,
  which provide `phpize` and `php-config`
- A C compiler, `autoconf` and `make`

On macOS with Homebrew, `brew install php` includes the headers. On
Debian/Ubuntu, install `php-dev` (or `php8.5-dev`) and `build-essential`.

### Build from source

```bash
git clone https://github.com/source-pot/php-function-autoloader.git
cd php-function-autoloader
phpize
./configure --enable-function-autoload
make
make test
```

The compiled extension ends up at `modules/function_autoload.so`.

### Install and enable

```bash
sudo make install
```

This copies the extension into your PHP extension directory
(`php-config --extension-dir`). Then enable it by adding this line to your
`php.ini`. Use `php --ini` to find the file, or drop the line into a new file
in the "additional .ini files" directory it lists.

```ini
extension=function_autoload
```

Check it is loaded:

```bash
php -m | grep function_autoload
```

### Try it without installing

You can load the extension for a single run without touching `php.ini`:

```bash
php -d extension=modules/function_autoload.so your-script.php
```

## Usage

### API

| Function | Description |
|---|---|
| `function_autoload_register(callable $callback, bool $prepend = false): true` | Add a loader. Registering the same loader twice has no effect. |
| `function_autoload_unregister(callable $callback): bool` | Remove a loader. Returns `false` if it was not registered. |
| `function_autoload_functions(): array` | List the registered loaders, in the order they run. |
| `function_autoload_call(string $function): bool` | Run the loaders for a name now. Returns whether the function is defined afterwards. |

A loader is any callable that takes the function name as a string. Its return
value is ignored. Loaders run in order until one of them defines the
function.

The name a loader receives:

- is fully qualified, without a leading backslash (`App\Helpers\today`)
- keeps the case used at the call site (function names in PHP are
  case-insensitive, so `TODAY()` passes `App\Helpers\TODAY`)

### What triggers autoloading

```php
namespace App\Controllers;

use function App\Helpers\today;

today();                          // imported with `use function`
\App\Helpers\today();             // fully qualified

$fn = 'App\Helpers\today';
$fn();                            // dynamic call with a string

call_user_func('App\Helpers\today');
call_user_func_array('App\Helpers\today', []);
function_exists('App\Helpers\today');   // loads it, then returns true
is_callable('App\Helpers\today');       // loads it, then returns true
```

Each call site resolves once. After that PHP uses its own runtime cache, so
the ongoing cost is a single cache check per call.

### Unqualified calls inside a namespace

When you call a function without a namespace or import inside a namespace,
PHP checks for the namespaced function first, then falls back to the global
one:

```php
namespace App\Controllers;

strlen('abc');  // tries App\Controllers\strlen, then \strlen
```

The extension never gets in the way of that fallback. Loaders only run when
**neither** function exists, so calls to built-in or already loaded functions
never reach your loader. When both are missing, the loaders are called with
the namespaced name first (`App\Controllers\helper`), then with the global
name (`helper`).

### Loader patterns

**One function per file.** This is what the example uses:
`App\Helpers\today` lives in `App/Helpers/today.php`.

```php
function_autoload_register(function (string $function): void {
    $file = __DIR__ . '/' . str_replace('\\', '/', $function) . '.php';

    if (is_file($file)) {
        require $file;
    }
});
```

**One file per namespace.** Keep all of a namespace's functions together, so
`App\Helpers\slugify` and `App\Helpers\excerpt` both live in
`src/Helpers.php`. A prefix map, like PSR-4, maps a namespace prefix to a
directory.

```php
$prefixes = ['App\\' => __DIR__ . '/src/'];

function_autoload_register(function (string $function) use ($prefixes): void {
    foreach ($prefixes as $prefix => $dir) {
        if (!str_starts_with($function, $prefix)) {
            continue;
        }

        $namespace = substr($function, strlen($prefix), strrpos($function, '\\') - strlen($prefix));
        $file = $dir . str_replace('\\', '/', $namespace) . '.php';

        if (is_file($file)) {
            require_once $file;
            return;
        }
    }
});
```

Use `require_once` here. If code asks for a function the file does not
contain, the loader runs again for the same file, and a plain `require` would
fail with "Cannot redeclare function".

**A static map.** The fastest option for production, because it skips the
filesystem checks. Generate the map at build time.

```php
$map = [
    'app\helpers\today' => __DIR__ . '/App/Helpers/today.php',
    'app\helpers\slugify' => __DIR__ . '/src/Helpers.php',
];

function_autoload_register(function (string $function) use ($map): void {
    $file = $map[strtolower($function)] ?? null;

    if ($file !== null) {
        require_once $file;
    }
});
```

The map keys are lowercased because function names are case-insensitive.

### Ordering and removing loaders

```php
function_autoload_register($fallbackLoader);
function_autoload_register($fastLoader, prepend: true);  // runs first

function_autoload_functions();  // [$fastLoader, $fallbackLoader]

function_autoload_unregister($fallbackLoader);
```

### Loading functions ahead of time

Some internal functions resolve string callables without going through any
point the extension can hook (see [Limitations](#known-limitations)). Load
the function first:

```php
function_autoload_call('App\Helpers\slugify');

array_map('App\Helpers\slugify', $titles);
```

Or pass a first-class callable instead, which triggers autoloading normally:

```php
array_map(\App\Helpers\slugify(...), $titles);
```

### Errors

If no loader defines the function, you get PHP's usual error:

```
Error: Call to undefined function App\Helpers\missing()
```

If a loader throws, the exception reaches the calling code unchanged:

```php
function_autoload_register(function (string $function): void {
    throw new RuntimeException("Not allowed to load $function");
});

try {
    \App\Helpers\today();
} catch (RuntimeException $e) {
    echo $e->getMessage();  // Not allowed to load App\Helpers\today
}
```

While a function is being autoloaded, asking for that same function again
(for example a loader calling `function_exists()` on it) does not run the
loaders a second time. It just reports the function as missing.

## Known limitations

- **The OPcache JIT is disabled** while this extension is loaded. PHP turns
  off the JIT for any extension that installs user opcode handlers, and prints
  a warning at startup if `opcache.jit` is enabled. OPcache itself still works
  normally.
- **String callables passed to other internal functions** (`array_map`,
  `usort`, `register_shutdown_function`, ...) are not autoloaded, because PHP
  looks them up with no point an extension can hook. Use
  `function_autoload_call()` or a first-class callable, as shown above.
- **Only tested on PHP 8.5 without thread safety (NTS).** The extension reads
  PHP's internal opcode layout, which can change between versions.

## Running the tests

```bash
make test
```

To run them with OPcache enabled:

```bash
make test TESTS="-d opcache.enable_cli=1 tests"
```

## How it works

PHP compiles a call to a function it does not know about yet into one of four
opcodes:

| Opcode | Used for |
|---|---|
| `ZEND_INIT_FCALL_BY_NAME` | Fully qualified calls and `use function` imports |
| `ZEND_INIT_NS_FCALL_BY_NAME` | Unqualified calls inside a namespace |
| `ZEND_INIT_DYNAMIC_CALL` | `$fn()` |
| `ZEND_INIT_USER_CALL` | `call_user_func()` when compiled to an opcode |

The extension registers handlers for these opcodes with
`zend_set_user_opcode_handler()`. If the call site has not been resolved yet
and the function is missing, it runs the loaders, then hands control back to
PHP's own handler, which finds the newly defined function. It also wraps
`function_exists()`, `is_callable()`, `call_user_func()` and
`call_user_func_array()` so they autoload before running.

All of it is in [`function_autoload.c`](function_autoload.c).
