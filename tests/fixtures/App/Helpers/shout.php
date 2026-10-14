<?php
namespace App\Helpers;

function shout(string $s): string {
    return strtoupper($s) . '!';
}
