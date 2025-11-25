/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SLOG_H
#define SLOG_H

#include <assert.h>
#include <limits.h>
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
	SLOG_TYPE_STRING = 1,
	SLOG_TYPE_INT,
	SLOG_TYPE_FLOAT,
	SLOG_TYPE_BOOL,
	SLOG_TYPE_TIME,
	SLOG_TYPE_ARRAY,
	SLOG_TYPE_OBJECT,
};

struct slog_node {
	enum slog_type type;

	const char *key;
	union {
		const char *string;
		double number;
		long long integer;
		bool boolean;
		enum slog_level level;
		struct timespec time;
		struct slog_node *array;
		struct slog_node *object;
	} value;

	struct slog_node *next;
};

const struct slog_node slog_node_default;

static thread_local struct slog_node *slog_node_thread_local = NULL;

struct slog_node *slog_node_get() {
	struct slog_node *node;

	if (slog_node_thread_local) {
		node = slog_node_thread_local;
		slog_node_thread_local = node->next;
	} else {
		node = calloc(1, sizeof(*node));
	}
	*node = slog_node_default;
	return node;
}

void slog_node_put(struct slog_node *node) {
	if (!node) {
		return;
	}
	node->next = slog_node_thread_local;
	slog_node_thread_local = node;
}

void slog_node_free(struct slog_node *node) {
	if (!node) {
		return;
	}
	slog_node_free(node->next);
	free(node);
}

typedef void (*slog_output_handler_t)(const char *);

static thread_local slog_output_handler_t slog_output_handler;

void SLOG_SET_HANDLER(slog_output_handler_t cb) {
	assert(cb);
	slog_output_handler = cb;
}

void SLOG_FREE(void) {
	slog_node_free(slog_node_thread_local);
	slog_node_thread_local = NULL;
	slog_output_handler = NULL;
}

const char *slog_buffer_write(const char *fmt, ...) {
	static thread_local char buffer[PIPE_BUF] = {0};
	static thread_local size_t index = 0; // without \0
	if (!fmt) {
		index = 0;
		return buffer;
	}

	if (index + 1 >= sizeof(buffer)) {
		fprintf(stderr, "Buffer full\n");
		return NULL;
	}

	va_list args;
	va_start(args, fmt); // vprintf(fmt, args);
	int len = vsnprintf(buffer + index, sizeof(buffer) - index, fmt, args);
	va_end(args);
	if (len > 0) {
		index += len;
	}
	return NULL;
}

struct slog_node *slog_node_make_list(bool clear_keys, va_list ap) {
	struct slog_node *head = NULL;
	struct slog_node **next_ptr = &head;
	struct slog_node *current;

	while ((current = va_arg(ap, struct slog_node *)) != NULL) {
		if (clear_keys) {
			current->key = NULL;
		}
		*next_ptr = current;
		next_ptr = &current->next;
	}

	return head;
}

struct slog_node *slog_node_vcreate(enum slog_type type, const char *key,
				    va_list ap) {
	struct slog_node *node = slog_node_get();
	node->key = key;
	node->type = type;

	switch (type) {
	case SLOG_TYPE_STRING:
		node->value.string = va_arg(ap, const char *);
		break;
	case SLOG_TYPE_INT:
		node->value.integer = va_arg(ap, long long);
		break;
	case SLOG_TYPE_FLOAT:
		node->value.number = va_arg(ap, double);
		break;
	case SLOG_TYPE_BOOL:
		node->value.boolean = va_arg(ap, int);
		break;
	case SLOG_TYPE_TIME:
		clock_gettime(CLOCK_REALTIME, &node->value.time);
		break;
	case SLOG_TYPE_ARRAY:
		node->value.array = slog_node_make_list(true, ap);
		break;
	case SLOG_TYPE_OBJECT:
		node->value.object = slog_node_make_list(false, ap);
		break;
	}
	return node;
}

struct slog_node *slog_node_create(enum slog_type type, const char *key, ...) {
	va_list ap;
	va_start(ap, key);
	struct slog_node *node = slog_node_vcreate(type, key, ap);
	va_end(ap);
	return node;
}

void slog_write_escape(const char *str) {
	assert(str);

	slog_buffer_write("\"");

	for (const char *p = str; *p; p++) {
		unsigned char c = *p;

		switch (c) {
		case '"':
			slog_buffer_write("\\\"");
			break;
		case '\\':
			slog_buffer_write("\\\\");
			break;
		case '\b':
			slog_buffer_write("\\b");
			break;
		case '\f':
			slog_buffer_write("\\f");
			break;
		case '\n':
			slog_buffer_write("\\n");
			break;
		case '\r':
			slog_buffer_write("\\r");
			break;
		case '\t':
			slog_buffer_write("\\t");
			break;
		default:
			if (c < 0x20) {
				slog_buffer_write("\\u%04x", c);
			} else {
				slog_buffer_write("%c", c);
			}
			break;
		}
	}
	slog_buffer_write("\"");
}

void slog_write_time(struct timespec *ts) {
	// unix timestamp in seconds.microseconds format
	// e.g. 1763456783.899468
	slog_buffer_write("\"%ld.%06lu\"", ts->tv_sec, ts->tv_nsec / 1000);
}

void slog_write_node(struct slog_node *node) {
	while (node) {
		struct slog_node *node_defer = node;
		if (node->key) {
			slog_write_escape(node->key);
			slog_buffer_write(":");
		}
		switch (node->type) {
		case SLOG_TYPE_STRING:
			slog_write_escape(node->value.string);
			break;
		case SLOG_TYPE_BOOL:
			slog_buffer_write("%s", node->value.boolean ? "true"
								    : "false");
			break;
		case SLOG_TYPE_INT:
			slog_buffer_write("%lld", node->value.integer);
			break;
		case SLOG_TYPE_FLOAT:
			slog_buffer_write("%f", node->value.number);
			break;
		case SLOG_TYPE_ARRAY:
			slog_buffer_write("[");
			slog_write_node(node->value.array);
			slog_buffer_write("]");
			break;
		case SLOG_TYPE_OBJECT:
			slog_buffer_write("{");
			slog_write_node(node->value.object);
			slog_buffer_write("}");
			break;
		case SLOG_TYPE_TIME:
			slog_write_time(&node->value.time);
			break;
		default:
			break;
		}
		node = node->next;
		if (node) {
			slog_buffer_write(",");
		}
		slog_node_put(node_defer);
	}
}

void slog_log_main(const char *file, const int line, const char *func,
		   const char *level, const char *msg, ...) {
	va_list nodes;
	va_start(nodes, msg);

	struct slog_node *extra_head = slog_node_make_list(false, nodes);
	va_end(nodes);

	if (strncmp(level, "SLOG_", 5) == 0) {
		level = level + 5;
	}

	struct slog_node *msg_node =
		slog_node_create(SLOG_TYPE_STRING, "msg", msg);
	struct slog_node *root = slog_node_create(
		SLOG_TYPE_OBJECT, NULL,
		slog_node_create(SLOG_TYPE_STRING, "file", file),
		slog_node_create(SLOG_TYPE_INT, "line", line),
		slog_node_create(SLOG_TYPE_STRING, "func", func),
		slog_node_create(SLOG_TYPE_STRING, "level", level),
		slog_node_create(SLOG_TYPE_TIME, "time"), msg_node, NULL);

	if (extra_head) {
		msg_node->next = extra_head;
	}

	slog_buffer_write(NULL);
	slog_write_node(root);
	const char *buffer = slog_buffer_write(NULL);

	if (slog_output_handler) {
		slog_output_handler(buffer);
	} else {
		fprintf(stdout, "%s\n", buffer);
		fflush(stdout);
	}
}

#define SLOG_BOOL(K, V) slog_node_create(SLOG_TYPE_BOOL, K, V)
#define SLOG_FLOAT(K, V) slog_node_create(SLOG_TYPE_FLOAT, K, V)
#define SLOG_STRING(K, V) slog_node_create(SLOG_TYPE_STRING, K, V)
#define SLOG_INT(K, V) slog_node_create(SLOG_TYPE_INT, K, V)
#define SLOG_ARRAY(K, ...)                                                     \
	slog_node_create(SLOG_TYPE_ARRAY, K __VA_OPT__(, ) __VA_ARGS__, NULL)
#define SLOG_OBJECT(K, ...)                                                    \
	slog_node_create(SLOG_TYPE_OBJECT, K __VA_OPT__(, ) __VA_ARGS__, NULL)

#define SLOG(LEVEL, MSG, ...)                                                  \
	do {                                                                   \
		slog_log_main(__FILE__, __LINE__, __func__, #LEVEL, MSG,       \
			      ##__VA_ARGS__, NULL);                            \
	} while (0)

#endif // SLOG_H
