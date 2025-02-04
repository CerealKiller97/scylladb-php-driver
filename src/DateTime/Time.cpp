/**
 * Copyright 2015-2017 DataStax, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ZendCPP/ZendCPP.hpp"
#include "php_driver.h"
#include "php_driver_types.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/types.h"

#if defined(__APPLE__) && defined(__MACH__)
#include <sys/time.h>
#else
#include <ctime>
#endif

#define NUM_NANOSECONDS_PER_DAY 86399999999999LL
#define NANOSECONDS_PER_SECOND 1000000000LL

cass_int64_t php_driver_time_now_ns() {
  cass_int64_t seconds = 0;
  cass_int64_t nanoseconds = 0;
#if defined(__APPLE__) && defined(__MACH__)
  struct timeval ts {};
  gettimeofday(&tv, NULL);
  seconds = (cass_int64_t)tv.tv_sec;
  nanoseconds = (cass_int64_t)tv.tv_usec * 1000;
#else
  struct timespec ts {};
  clock_gettime(CLOCK_REALTIME, &ts);
  seconds = (cass_int64_t)ts.tv_sec;
  nanoseconds = (cass_int64_t)ts.tv_nsec;
#endif
  return cass_time_from_epoch(seconds) + nanoseconds;
}

BEGIN_EXTERN_C()
#include <ext/date/lib/timelib.h>
#include <ext/date/php_date.h>

#include "Time_arginfo.h"

zend_class_entry *php_driver_time_ce = nullptr;

static int to_string(zval *result, php_driver_time *time) {
  char *string;
  spprintf(&string, 0, "%lld", (long long int)time->time);
  ZVAL_STRING(result, string);
  efree(string);
  return SUCCESS;
}

void php_driver_time_init(INTERNAL_FUNCTION_PARAMETERS) {
  zval *nanoseconds = nullptr;
  php_driver_time *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &nanoseconds) == FAILURE) {
    return;
  }

  if (getThis() && instanceof_function(Z_OBJCE_P(getThis()), php_driver_time_ce)) {
    self = PHP_DRIVER_GET_TIME(getThis());
  } else {
    object_init_ex(return_value, php_driver_time_ce);
    self = PHP_DRIVER_GET_TIME(return_value);
  }

  if (nanoseconds == nullptr) {
    self->time = php_driver_time_now_ns();
  } else {
    if (Z_TYPE_P(nanoseconds) == IS_LONG) {
      self->time = Z_LVAL_P(nanoseconds);
    } else if (Z_TYPE_P(nanoseconds) == IS_STRING) {
      if (!php_driver_parse_bigint(Z_STRVAL_P(nanoseconds), Z_STRLEN_P(nanoseconds), &self->time)) {
        return;
      }
    } else {
      INVALID_ARGUMENT(nanoseconds,
                       "a string or int representing a number of nanoseconds "
                       "since midnight");
    }

    if (self->time < 0 || self->time > NUM_NANOSECONDS_PER_DAY) {
      INVALID_ARGUMENT(nanoseconds, "nanoseconds since midnight");
    }
  }
}

/* {{{ Time::__construct(string) */
ZEND_METHOD(Cassandra_Time, __construct) { php_driver_time_init(INTERNAL_FUNCTION_PARAM_PASSTHRU); }
/* }}} */

/* {{{ Time::type() */
ZEND_METHOD(Cassandra_Time, type) {
  php5to7_zval type = php_driver_type_scalar(CASS_VALUE_TYPE_TIME);
  RETURN_ZVAL(&type, 1, 1);
}
/* }}} */

/* {{{ Time::seconds() */
ZEND_METHOD(Cassandra_Time, seconds) {
  php_driver_time *self = PHP_DRIVER_GET_TIME(getThis());
  RETURN_LONG(self->time / NANOSECONDS_PER_SECOND);
}
/* }}} */

/* {{{ Time::fromDateTime() */
ZEND_METHOD(Cassandra_Time, fromDateTime) {
  php_driver_time *self;
  zval *zdatetime;
  php5to7_zval retval;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zdatetime) == FAILURE) {
    return;
  }

  zend_call_method_with_0_params(Z_OBJ_P(zdatetime), php_date_get_date_ce(), nullptr,
                                 "gettimestamp", &retval);

  if (!Z_ISUNDEF(retval) && Z_TYPE(retval) == IS_LONG) {
    object_init_ex(return_value, php_driver_time_ce);
    self = PHP_DRIVER_GET_TIME(return_value);
    self->time = cass_time_from_epoch(Z_LVAL(retval));
    zval_ptr_dtor(&retval);
  }
}
/* }}} */

/* {{{ Time::__toString() */
ZEND_METHOD(Cassandra_Time, __toString) {
  php_driver_time *self;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TIME(getThis());
  to_string(return_value, self);
}
/* }}} */

static php_driver_value_handlers php_driver_time_handlers;

static HashTable *php_driver_time_gc(zend_object *object, zval **table, int *n) {
  *table = nullptr;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *php_driver_time_properties(zend_object *object) {
  php5to7_zval type;
  php5to7_zval nanoseconds;

  php_driver_time *self = PHP5TO7_ZEND_OBJECT_GET(time, object);
  HashTable *props = zend_std_get_properties(object);

  type = php_driver_type_scalar(CASS_VALUE_TYPE_TIME);
  PHP5TO7_ZEND_HASH_UPDATE(props, "type", sizeof("type"), &type, sizeof(zval));

  to_string(&nanoseconds, self);
  PHP5TO7_ZEND_HASH_UPDATE(props, "nanoseconds", sizeof("nanoseconds"), &nanoseconds, sizeof(zval));

  return props;
}

static int php_driver_time_compare(zval *obj1, zval *obj2) {
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  php_driver_time *time1 = nullptr;
  php_driver_time *time2 = nullptr;
  if (Z_OBJCE_P(obj1) != Z_OBJCE_P(obj2)) return 1; /* different classes */

  time1 = PHP_DRIVER_GET_TIME(obj1);
  time2 = PHP_DRIVER_GET_TIME(obj2);

  return PHP_DRIVER_COMPARE(time1->time, time2->time);
}

static unsigned php_driver_time_hash_value(zval *obj) {
  php_driver_time *self = PHP_DRIVER_GET_TIME(obj);
  return php_driver_bigint_hash(self->time);
}

static php5to7_zend_object php_driver_time_new(zend_class_entry *ce) {
  auto *self = ZendCPP::Allocate<php_driver_time>(ce, &php_driver_time_handlers);
  self->time = 0;

  return &self->zval;
}

void php_driver_define_Time() {
  php_driver_time_ce = register_class_Cassandra_Time(php_driver_value_ce);
  php_driver_time_ce->create_object = php_driver_time_new;

  ZendCPP::InitHandlers(&php_driver_time_handlers);
  php_driver_time_handlers.std.get_properties = php_driver_time_properties;
  php_driver_time_handlers.std.get_gc = php_driver_time_gc;
  php_driver_time_handlers.std.compare = php_driver_time_compare;
  php_driver_time_handlers.std.offset = XtOffsetOf(php_driver_time, zval);
  php_driver_time_handlers.hash_value = php_driver_time_hash_value;
}
END_EXTERN_C()