<?php

/**
* @generate-class-entries
*/
namespace Cassandra {
    /**
     * @strict-properties
     */
    final class Timestamp implements Value {
        public function __construct(int $seconds = UNKNOWN, int $microseconds = UNKNOWN) {}

        public function type(): Type {}
        public function time(): int {}
        public function microtime(bool $get_as_float = false): float|string {}
        public function toDateTime(): \DateTime {}
        public static function fromDateTime(\DateTimeInterface $datetime): Timestamp {}

        public function __toString(): string {}
    }
}