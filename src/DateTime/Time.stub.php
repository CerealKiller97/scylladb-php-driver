<?php

/**
* @generate-class-entries
*/
namespace Cassandra {
    /**
     * @strict-properties
     */
    final class Time implements Value {
        public function __construct(int $nanoseconds = UNKNOWN) {}

        public function type(): Type\Scalar {}
        public function seconds(): int {}
        public static function fromDateTime(\DateTimeInterface $datetime): Time {}

        public function __toString(): string {}
    }
}