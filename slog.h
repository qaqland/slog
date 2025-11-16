/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SLOG_H
#define SLOG_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

enum slog_level {
	SLOG_ERROR = 0,
	SLOG_WARN = 1,
	SLOG_INFO = 2,
	SLOG_DEBUG = 3,
	SLOG_LAST
};

enum slog_type {
	SLOG_TYPE_NULL = 0,
	SLOG_TYPE_STRING,
	SLOG_TYPE_INT,
	SLOG_TYPE_FLOAT,
	SLOG_TYPE_BOOL,
	SLOG_TYPE_TIME,
	// SLOG_ARRAY,··
	SLOG_TYPE_OBJECT,
	SLOG_TYPE_PLAIN,
};

struct slog_field {
	enum slog_type type;

	const char *key;
	union {
		const char *string;
		double number;
		long long integer;
		bool boolean;
		enum slog_level level;
		struct timespec time;
		// struct slog_field *array;
		struct slog_field *object;
	} value;

	struct slog_field *next;
};

const struct slog_field slog_field_default;

static thread_local struct slog_field *slog_field_tls = NULL;

#ifndef SLOG_OUTPUT_SIZE
#define SLOG_OUTPUT_SIZE 4096
#endif
static thread_local char output_buffer[SLOG_OUTPUT_SIZE] = {0};
static thread_local size_t output_index = 1; // strlen() + 1

struct slog_field *slog_field_get() {
	struct slog_field *field;

	if (slog_field_tls) {
		field = slog_field_tls;
		slog_field_tls = field->next;
	} else {
		field = calloc(1, sizeof(*field));
	}
	*field = slog_field_default;
	return field;
}

void slog_field_put(struct slog_field *field) {
	if (!field) {
		return;
	}
	field->next = slog_field_tls;
	slog_field_tls = field;
}

void slog_free(struct slog_field *field) {
	if (!field) {
		return;
	}
	slog_free(field->next);
	free(field);
}

void SLOG_FREE(void) { slog_free(slog_field_tls); }

void slog_vfmt(const char *fmt, ...) {
	if (!fmt || output_index >= SLOG_OUTPUT_SIZE) {
		fprintf(stderr, "please increase SLOG_OUTPUT_SIZE");
		return;
	}

	va_list args;
	va_start(args, fmt);
	// vprintf(fmt, args);
	int written = vsnprintf(output_buffer + output_index - 1,
				SLOG_OUTPUT_SIZE - output_index + 1, fmt, args);
	va_end(args);
	if (written > 0) {
		output_index += written;
	}
}

struct slog_field *slog_field_vnew(enum slog_type type, const char *key,
				   va_list ap) {
	struct slog_field *field = slog_field_get();
	field->key = key;
	field->type = type;

	struct slog_field *p, **pp;

	switch (type) {
	case SLOG_TYPE_NULL:
		break;
	case SLOG_TYPE_STRING:
		field->value.string = va_arg(ap, const char *);
		break;
	case SLOG_TYPE_INT:
		field->value.integer = va_arg(ap, long long);
		break;
	case SLOG_TYPE_FLOAT:
		field->value.number = va_arg(ap, double);
		break;
	case SLOG_TYPE_BOOL:
		field->value.boolean = va_arg(ap, int);
		break;
	case SLOG_TYPE_TIME:
		clock_gettime(CLOCK_REALTIME, &field->value.time);
		break;
	// case SLOG_ARRAY:
	case SLOG_TYPE_PLAIN:
		assert(!key);
	case SLOG_TYPE_OBJECT:
		pp = &field->value.object;
		while ((p = va_arg(ap, struct slog_field *)) != NULL) {
			*pp = p;
			pp = &p->next;
		}
		break;
	}
	return field;
}

struct slog_field *slog_field_new(enum slog_type type, const char *key, ...) {
	va_list ap;
	va_start(ap, key);
	struct slog_field *field = slog_field_vnew(type, key, ap);
	va_end(ap);
	return field;
}

void slog_fmt_escape(const char *str) {
	assert(str);

	slog_vfmt("\"");

	for (const char *p = str; *p; p++) {
		unsigned char c = *p;

		switch (c) {
		case '"':
			slog_vfmt("\\\"");
			break;
		case '\\':
			slog_vfmt("\\\\");
			break;
		case '\b':
			slog_vfmt("\\b");
			break;
		case '\f':
			slog_vfmt("\\f");
			break;
		case '\n':
			slog_vfmt("\\n");
			break;
		case '\r':
			slog_vfmt("\\r");
			break;
		case '\t':
			slog_vfmt("\\t");
			break;
		default:
			if (c < 0x20) {
				slog_vfmt("\\u%04x", c);
			} else {
				slog_vfmt("%c", c);
			}
			break;
		}
	}
	slog_vfmt("\"");
}

void slog_fmt_time(struct slog_field *field) {
	assert(field->type == SLOG_TYPE_TIME);

	struct tm *t = localtime(&field->value.time.tv_sec);

	// YYYY-MM-DD HH:MM:SS.mmm
	// 2025-11-16 11:15:25.123

	slog_vfmt("\"time\": \"%04d-%02d-%02d %02d:%02d:%02d.%03ld\"",
		  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
		  t->tm_min, t->tm_sec, field->value.time.tv_nsec / 1000000);
}

void slog_field_fmt(struct slog_field *field) {
	while (field) {
		struct slog_field *field_bk = field;
		if (field->key) {
			slog_fmt_escape(field->key);
			slog_vfmt(": ");
		}
		switch (field->type) {
		case SLOG_TYPE_STRING:
			slog_fmt_escape(field->value.string);
			break;
		case SLOG_TYPE_BOOL:
			slog_vfmt("%s",
				  field->value.boolean ? "true" : "false");
			break;
		case SLOG_TYPE_INT:
			slog_vfmt("%lld", field->value.integer);
			break;
		case SLOG_TYPE_FLOAT:
			slog_vfmt("%f", field->value.number);
			break;
		case SLOG_TYPE_OBJECT:
			slog_vfmt("{");
			slog_field_fmt(field->value.object);
			slog_vfmt("}");
			break;
		case SLOG_TYPE_PLAIN:
			assert(!field->key);
			slog_field_fmt(field->value.object);
			break;
		case SLOG_TYPE_TIME:
			slog_fmt_time(field);
			break;
		default:
			break;
		}
		field = field->next;
		if (field) {
			slog_vfmt(", ");
		}
		slog_field_put(field_bk);
	}
}

static void slog_main(const char *file, const int line, const char *func,
		      const char *level, const char *msg, ...) {
	va_list fields;
	va_start(fields, msg);
	struct slog_field *extra =
		slog_field_vnew(SLOG_TYPE_PLAIN, NULL, fields);
	va_end(fields);

	if (strncmp(level, "SLOG_", 5) == 0) {
		level = level + 5;
	}

	struct slog_field *root = slog_field_new(
		SLOG_TYPE_OBJECT, NULL,
		slog_field_new(SLOG_TYPE_STRING, "file", file),
		slog_field_new(SLOG_TYPE_INT, "line", line),
		slog_field_new(SLOG_TYPE_STRING, "func", func),
		slog_field_new(SLOG_TYPE_STRING, "level", level),
		slog_field_new(SLOG_TYPE_STRING, "msg", msg),
		slog_field_new(SLOG_TYPE_TIME, NULL), extra, NULL);

	output_index = 1;
	output_buffer[0] = '\0';

	slog_field_fmt(root);

	fprintf(stdout, "%s\n", output_buffer);
	fflush(stdout);
}

#define SLOG_BOOL(K, V) slog_field_new(SLOG_TYPE_BOOL, K, V)
#define SLOG_FLOAT(K, V) slog_field_new(SLOG_TYPE_FLOAT, K, V)
#define SLOG_STRING(K, V) slog_field_new(SLOG_TYPE_STRING, K, V)
#define SLOG_INT(K, V) slog_field_new(SLOG_TYPE_INT, K, V)
#define SLOG_OBJECT(K, ...)                                                    \
	slog_field_new(SLOG_TYPE_OBJECT, K __VA_OPT__(, ) __VA_ARGS__, NULL)

#define SLOG(LEVEL, MSG, ...)                                                  \
	do {                                                                   \
		slog_main(__FILE__, __LINE__, __func__, #LEVEL, MSG,           \
			  ##__VA_ARGS__, NULL);                                \
	} while (0)

#endif // SLOG_H
