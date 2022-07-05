#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "Zend/zend_observer.h"
#include "php_observer.h"
#include "khash.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#define DUMP_ZVAL_MAX_LEN 2048

static inline double gettime() __attribute__((always_inline));
static inline double gettime()
{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec + (double)tv.tv_usec*1e-6;
}

KHASH_MAP_INIT_INT64(m64, double);

khash_t(m64) *g_funtime_h;

ZEND_DECLARE_MODULE_GLOBALS(observer)

int g_silent = 0;

static void g_toggle_verbosity(int signum) {
	g_silent = !g_silent;
}

static void g_toggle_dump_args(int signum) {
	OBSERVER_G(instrument_dump_arguments) = !OBSERVER_G(instrument_dump_arguments);
}

static char *dump_zval(zval *zv, int limit) {
	if (limit < 0) {
		return "*recursion*";
	}
	if (limit > 7)
		limit = 7;
	int tlen = 0;
	static char buf[8][DUMP_ZVAL_MAX_LEN], *tstr = NULL;
	int offset = 0;
	offset = snprintf(buf[limit], sizeof buf[limit], "(%s)", zend_zval_type_name(zv));
	int blen = sizeof buf[0] - offset - 1;
	int new_offset=0;
	HashTable *myht;
	zend_ulong index;
	zend_string *key;
	zval *val;

	

again:
		switch (Z_TYPE_P(zv)) {
			case IS_TRUE:
				return "true";
			case IS_FALSE:
				return "false";
			case IS_NULL:
				return "NULL";
			case IS_LONG:
				snprintf(buf[limit] + offset, blen, "%ld", Z_LVAL_P(zv));
				return buf[limit];
			case IS_DOUBLE:
				snprintf(buf[limit] + offset, blen, "%.*G", (int) EG(precision), Z_DVAL_P(zv));
				return buf[limit];
			case IS_STRING:
				tlen = (limit <= 0 || Z_STRLEN_P(zv) < limit) ? Z_STRLEN_P(zv) : limit;
				snprintf(buf[limit] +offset , blen, "\"%*s\"", tlen, Z_STRVAL_P(zv));
				return buf[limit];
			case IS_ARRAY:
				myht = Z_ARRVAL_P(zv);
				new_offset = snprintf(buf[limit]+offset, blen, "[");
				offset += new_offset;
				blen -= new_offset;
				ZEND_HASH_FOREACH_KEY_VAL_IND(myht, index, key, val) {
					if (key) {
						new_offset = snprintf(buf[limit]+offset, blen, "%s => %s,", ZSTR_VAL(key), dump_zval(val, limit-1));
					} else {
						new_offset = snprintf(buf[limit]+offset, blen, "%d => %s,", index, dump_zval(val, limit-1));
					}
					offset += new_offset;
					blen -= new_offset;
				} ZEND_HASH_FOREACH_END();
				new_offset = snprintf(buf[limit]+offset, blen, "]");
				offset += new_offset;
				snprintf(buf[limit]+offset, blen, "#%d", zend_hash_num_elements(Z_ARRVAL_P(zv)));
				return buf[limit];
			case IS_OBJECT:
				snprintf(buf[limit]+offset, blen, "#%d", Z_OBJ_HANDLE_P(zv));
				return buf[limit];
			case IS_RESOURCE:
				tstr = (char *) zend_rsrc_list_get_rsrc_type(Z_RES_P(zv));
				snprintf(buf[limit]+offset, blen, "{%s}#%d", tstr?tstr:"Unknown", Z_RES_P(zv)->handle);
				return buf[limit];
			case IS_UNDEF:
				return "{undef}";
			case IS_REFERENCE:
				zv = Z_REFVAL_P(zv);
				goto again;
			default:
				return "{unknown}";
		}
}

char *dump_call_trace(zend_execute_data *execute_data) {
	static char buffer[4096];
	const char *class_name = NULL;
	const char *function_name;
	const char *filename = NULL;
	char *call_type = NULL;
	int lineno = 0;

	zend_object *object;
	zend_function *func;


	object = (Z_TYPE(execute_data->This) == IS_OBJECT) ? Z_OBJ(execute_data->This) : NULL;

	if (execute_data->func) {
		func = execute_data->func;
		if (func->common.function_name) {
			function_name = ZSTR_VAL(func->common.function_name);
		} else {
			function_name = NULL;
		}
	} else {
		func = NULL;
		function_name = NULL;
	}

	if (function_name) {
		if (object) {
			if (func->common.scope) {
				class_name = ZSTR_VAL(func->common.scope->name);
			} else if (object->handlers->get_class_name == zend_std_get_class_name) {
				class_name = ZSTR_VAL(object->ce->name);
			} else {
				class_name = ZSTR_VAL(object->handlers->get_class_name(object));
			}
			call_type = "->";
		} else if (func->common.scope) {
			class_name = ZSTR_VAL(func->common.scope->name);
			call_type = "::";
		} else {
			class_name = NULL;
			call_type = NULL;
		}
	}

	filename =	ZSTR_VAL(execute_data->func->op_array.filename);
	lineno =	execute_data->opline->lineno;

	static char fun_args[2048]; fun_args[0] = 0;

	if (OBSERVER_G(instrument_dump_arguments)) {
		int offset = 0;
		int num_args = ZEND_CALL_NUM_ARGS(execute_data);
		if (num_args > 0) {
			int i=0;
			zval *p = ZEND_CALL_ARG(execute_data, 1);
			if (execute_data->func->type == ZEND_USER_FUNCTION) {
				uint32_t first_extra_arg = execute_data->func->op_array.num_args;

				if (first_extra_arg && num_args > first_extra_arg) {
					while (i < first_extra_arg) {
						offset += snprintf(fun_args+offset, sizeof fun_args - offset -1, "(extra)%s,", dump_zval(p++, 7));
						i++;
					}
					p = ZEND_CALL_VAR_NUM(execute_data, execute_data->func->op_array.last_var + execute_data->func->op_array.T);
				}
			}
			while (i<num_args) {
				offset += snprintf(fun_args+offset, sizeof fun_args - offset -1, "%s,", dump_zval(p++, 7));
				i++;
			}
		}
	}

	if (class_name) {
		snprintf(buffer, sizeof buffer, "%s%s%s(%s) [%s:%d]", class_name, call_type, function_name, fun_args, filename, lineno);
	} else if (function_name) {
		snprintf(buffer, sizeof buffer, "%s(%s) [%s:%d]", function_name, fun_args, filename, lineno);
	}

	return buffer;
}

int indent = 0;
static void observer_begin(zend_execute_data *execute_data) {
	if (!g_silent) {
		fprintf(stderr, "%*s %s", indent+1, ">", dump_call_trace(execute_data));

		if (OBSERVER_G(instrument_dump_execute_data_ptr)) {
      fprintf(stderr, " 0x%" PRIXPTR, (uintptr_t)execute_data);
    }

    fprintf(stderr, "\n");

		int absent;
		khint_t k;
		k = kh_put(m64, g_funtime_h, (int64_t)execute_data, &absent);
		kh_value(g_funtime_h, k) = gettime();
	}

	indent += 2;
}

static void observer_end(zend_execute_data *execute_data, zval *return_value) {
	indent -= 2;

	if (!g_silent) {
		khint_t k;
		k = kh_get(m64, g_funtime_h, (int64_t)execute_data);
		double time_taken = (gettime() - kh_value(g_funtime_h, k)) * 1000;
		kh_del(m64, g_funtime_h, k);
		static char *nulllbl = "null";
		const char *pp_return_value;
		if (return_value == NULL) {
				pp_return_value = nulllbl;
		} else if (OBSERVER_G(instrument_dump_arguments)) {
			static char zval_repr[DUMP_ZVAL_MAX_LEN];
			zval_repr[0] = 0;
			strcpy(zval_repr, dump_zval(return_value, 7));
			pp_return_value = zval_repr;
		} else {
			pp_return_value = zend_zval_type_name(return_value);
		}

		fprintf(stderr, "%*s %s: %s", indent+1, "<",
				dump_call_trace(execute_data),
				return_value ? pp_return_value: "null",
				pp_return_value);

		if (OBSERVER_G(instrument_dump_execute_data_ptr)) {
      fprintf(stderr, " 0x%" PRIXPTR, (uintptr_t)execute_data);
    }

		if (OBSERVER_G(instrument_dump_timing)) {
      fprintf(stderr, " (%f)", time_taken);
    }

    fprintf(stderr, "\n");
	}
}

// Runs once per zend_function on its first call
static zend_observer_fcall_handlers observer_instrument(zend_execute_data *execute_data) {
	zend_observer_fcall_handlers handlers = {NULL, NULL};

	if (OBSERVER_G(instrument) == 0 ||
		!execute_data->func ||
		!execute_data->func->common.function_name) {
		return handlers; // I have no handlers for this function
	}
	handlers.begin = observer_begin;
	handlers.end = observer_end;
	return handlers; // I have handlers for this function
}

static void php_observer_init_globals(zend_observer_globals *observer_globals)
{
	observer_globals->instrument = 0;
	observer_globals->instrument_dump_arguments = 1;
	observer_globals->instrument_dump_timing = 1;
	observer_globals->instrument_dump_execute_data_ptr = 1;
	g_funtime_h = kh_init(m64);
	signal(SIGUSR1, g_toggle_verbosity);
	signal(SIGUSR2, g_toggle_dump_args);
}

PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("observer.instrument", "0", PHP_INI_SYSTEM, OnUpdateBool, instrument, zend_observer_globals, observer_globals)
	STD_PHP_INI_BOOLEAN("observer.instrument_dump_arguments", "1", PHP_INI_SYSTEM, OnUpdateBool, instrument_dump_arguments, zend_observer_globals, observer_globals)
	STD_PHP_INI_BOOLEAN("observer.instrument_dump_timing", "1", PHP_INI_SYSTEM, OnUpdateBool, instrument_dump_timing, zend_observer_globals, observer_globals)
	STD_PHP_INI_BOOLEAN("observer.instrument_dump_execute_data_ptr", "1", PHP_INI_SYSTEM, OnUpdateBool, instrument_dump_execute_data_ptr, zend_observer_globals, observer_globals)
PHP_INI_END()

static PHP_MINIT_FUNCTION(observer)
{
	ZEND_INIT_MODULE_GLOBALS(observer, php_observer_init_globals, NULL);
	REGISTER_INI_ENTRIES();
	zend_observer_fcall_register(observer_instrument);

	return SUCCESS;
}

static PHP_RINIT_FUNCTION(observer)
{
#if defined(ZTS) && defined(COMPILE_DL_OBSERVER)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

static PHP_MINFO_FUNCTION(observer)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Observer PoC extension", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

zend_module_entry observer_module_entry = {
	STANDARD_MODULE_HEADER,
	"observer",
	NULL,					/* zend_function_entry */
	PHP_MINIT(observer),
	NULL,					/* PHP_MSHUTDOWN */
	PHP_RINIT(observer),
	NULL,					/* PHP_RSHUTDOWN */
	PHP_MINFO(observer),
	PHP_OBSERVER_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OBSERVER
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(observer)
#endif
