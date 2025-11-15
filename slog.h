#ifndef SLOG_H
#define SLOG_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define _STR(VAR) #VAR

static FILE *SLOG_OUTPUT = NULL;
#define SLOG_SET_OUTPUT(FILE_PTR) SLOG_OUTPUT = FILE_PTR

enum slog_level {
	SLOG_TRACE,
	SLOG_DEBUG,
	SLOG_INFO,
	SLOG_WARN,
	SLOG_ERROR,
	SLOG_FATAL
};

enum slog_type {
	SLOG_NULL,
	SLOG_STR,
	SLOG_INT,
	SLOG_FLOAT,
	SLOG_BOOL,
	SLOG_TIMESTAMP,
	SLOG_LEVEL,
	SLOG_ARRAY,
	SLOG_GROUP,
	SLOG_CLOSE,
};

struct slog_list {
	struct slog_list *next;
	struct slog_list *prev;
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
	} value; // null

	struct slog_list link;
};

const struct slog_field slog_field_default;

struct slog_ctx {
	FILE *output;

	// it's hard to share context in one same file
	// enum slog_level level;
	// struct slog_field *with;

	// one output
	struct slog_field *pool;
	size_t length, capacity;
};

struct slog_ctx slog_ctx_default;

#define slog_container_of(ptr, sample, member)                                 \
	(void *) ((char *) (ptr) -                                             \
		  ((char *) &(sample)->member - (char *) (sample)))

#define slog_list_for_each(pos, head, member)                                  \
	for (pos = 0, pos = slog_container_of((head)->next, pos, member);      \
	     &pos->member != (head);                                           \
	     pos = slog_container_of(pos->member.next, pos, member))

void slog_list_init(struct slog_list *list) {
	list->next = list;
	list->prev = list;
}

void slog_list_append(struct slog_list *list, struct slog_list *append) {
	struct slog_list *append_end = append->prev;
	struct slog_list *list_end = list->prev;

	append->prev = list_end;
	list_end->next = append;

	append_end->next = list;
	list->prev = append_end;
}

struct slog_field *slog_new_field(struct slog_ctx *context, enum slog_type type,
				  const char *key) {
	assert(context);
	if (context->capacity == 0 || context->capacity == context->length) {
		return NULL;
	}
	// struct slog_field *field = calloc(1, sizeof(*field));
	struct slog_field *field = &context->pool[context->length++];
	*field = slog_field_default;

	slog_list_init(&field->link);
	field->key = key;
	field->type = type;
	return field;
}

struct slog_field *slog_new_int(struct slog_ctx *context, const char *key,
				long long value) {
	struct slog_field *field = slog_new_field(context, SLOG_INT, key);
	field->value.integer = value;
	return field;
}

struct slog_field *slog_new_str(struct slog_ctx *context, const char *key,
				const char *value) {
	struct slog_field *field = slog_new_field(context, SLOG_STR, key);
	field->value.string = value;
	return field;
}

struct slog_field *slog_new_float(struct slog_ctx *context, const char *key,
				  double value) {
	struct slog_field *field = slog_new_field(context, SLOG_FLOAT, key);
	field->value.number = value;
	return field;
}

struct slog_field *slog_new_bool(struct slog_ctx *context, const char *key,
				 bool value) {
	struct slog_field *field = slog_new_field(context, SLOG_BOOL, key);
	field->value.boolean = value;
	return field;
}

static struct slog_field *slog_new_group(struct slog_ctx *context,
					 const char *key, ...) {
	struct slog_field *start = slog_new_field(context, SLOG_GROUP, key);

	va_list args;
	va_start(args, key);
	struct slog_field *element;
	while ((element = va_arg(args, struct slog_field *)),
	       element != SLOG_NULL) {
		slog_list_append(&start->link, &element->link);
	}
	va_end(args);

	struct slog_field *end = slog_new_field(context, SLOG_CLOSE, "}");
	slog_list_append(&start->link, &end->link);

	return start;
}

void slog_fprintf_escape(FILE *stream, const char *str) {
	assert(str);

	fputc('"', stream);

	for (const char *p = str; *p; p++) {
		unsigned char c = *p;

		switch (c) {
		case '"':
			fputs("\\\"", stream);
			break;
		case '\\':
			fputs("\\\\", stream);
			break;
		case '\b':
			fputs("\\b", stream);
			break;
		case '\f':
			fputs("\\f", stream);
			break;
		case '\n':
			fputs("\\n", stream);
			break;
		case '\r':
			fputs("\\r", stream);
			break;
		case '\t':
			fputs("\\t", stream);
			break;
		default:
			if (c < 0x20) {
				fprintf(stream, "\\u%04x", c);
			} else {
				fputc(c, stream);
			}
			break;
		}
	}

	fputc('"', stream);
}

void slog_fprintf_str(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_STR);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	slog_fprintf_escape(SLOG_OUTPUT, field->key);
	fprintf(SLOG_OUTPUT, ": ");
	slog_fprintf_escape(SLOG_OUTPUT, field->value.string);

	*comma = true;
}

void slog_fprintf_float(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_FLOAT);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	slog_fprintf_escape(SLOG_OUTPUT, field->key);
	fprintf(SLOG_OUTPUT, ": %f", field->value.number);

	*comma = true;
}

void slog_fprintf_int(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_INT);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	slog_fprintf_escape(SLOG_OUTPUT, field->key);
	fprintf(SLOG_OUTPUT, ": %lld", field->value.integer);

	*comma = true;
}

void slog_fprintf_bool(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_BOOL);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	slog_fprintf_escape(SLOG_OUTPUT, field->key);
	fprintf(SLOG_OUTPUT, ": %s", field->value.boolean ? "true" : "false");

	*comma = true;
}

void slog_fprintf_time(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_TIMESTAMP);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	fprintf(SLOG_OUTPUT, "\"timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\"",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
		t->tm_min, t->tm_sec);

	*comma = true;
}

void slog_fprintf_group(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_GROUP);

	if (!field->key) {
		fprintf(SLOG_OUTPUT, "{");
		return;
	}

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	slog_fprintf_escape(SLOG_OUTPUT, field->key);
	fprintf(SLOG_OUTPUT, ": {");

	*comma = false;
}

void slog_fprintf_close(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_CLOSE);

	fprintf(SLOG_OUTPUT, "%s", field->key);

	*comma = true;
}

void slog_fprintf_level(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_LEVEL);

	if (*comma) {
		fprintf(SLOG_OUTPUT, ", ");
	}

	fprintf(SLOG_OUTPUT, "\"level\": ");
	switch (field->value.level) {
	case SLOG_TRACE:
		fprintf(SLOG_OUTPUT, "\"trace\"");
		break;
	case SLOG_DEBUG:
		fprintf(SLOG_OUTPUT, "\"debug\"");
		break;
	case SLOG_INFO:
		fprintf(SLOG_OUTPUT, "\"info\"");
		break;
	case SLOG_WARN:
		fprintf(SLOG_OUTPUT, "\"warn\"");
		break;
	case SLOG_ERROR:
		fprintf(SLOG_OUTPUT, "\"error\"");
		break;
	case SLOG_FATAL:
		fprintf(SLOG_OUTPUT, "\"fatal\"");
		break;
	}

	*comma = true;
}

void slog_fprintf_null(struct slog_field *field, bool *comma) {
	assert(field->type == SLOG_NULL);
	fprintf(SLOG_OUTPUT, "\n");
	fflush(SLOG_OUTPUT);

	*comma = false;
}

void slog_fprintf_field(struct slog_field *field) {
	static bool want_comma = false;

	switch (field->type) {
	case SLOG_STR:
		return slog_fprintf_str(field, &want_comma);
	case SLOG_BOOL:
		return slog_fprintf_bool(field, &want_comma);
	case SLOG_INT:
		return slog_fprintf_int(field, &want_comma);
	case SLOG_FLOAT:
		return slog_fprintf_float(field, &want_comma);
	case SLOG_GROUP:
		return slog_fprintf_group(field, &want_comma);
	case SLOG_CLOSE:
		return slog_fprintf_close(field, &want_comma);
	case SLOG_TIMESTAMP:
		return slog_fprintf_time(field, &want_comma);
	case SLOG_LEVEL:
		return slog_fprintf_level(field, &want_comma);
	case SLOG_NULL:
		return slog_fprintf_null(field, &want_comma);
	default:
		break;
	}
}

#define SLOG_CTX_VAR slog_ctx_var
#define S_BOOL(KEY, VALUE) slog_new_bool(&SLOG_CTX_VAR, KEY, VALUE)
#define S_FLOAT(KEY, VALUE) slog_new_float(&SLOG_CTX_VAR, KEY, VALUE)
#define S_STR(KEY, VALUE) slog_new_str(&SLOG_CTX_VAR, KEY, VALUE)
#define S_INT(KEY, VALUE) slog_new_int(&SLOG_CTX_VAR, KEY, VALUE)

#define S_GROUP(KEY, ...)                                                      \
	slog_new_group(&SLOG_CTX_VAR, KEY __VA_OPT__(, ) __VA_ARGS__, SLOG_NULL)

static void slog_main(struct slog_ctx *ctx, enum slog_level level,
		      const char *msg, ...) {

	struct slog_list head = {0};
	slog_list_init(&head);

	struct slog_field *field_time =
		slog_new_field(ctx, SLOG_TIMESTAMP, "timestamp");

	struct slog_field *field_message = slog_new_str(ctx, "message", msg);

	struct slog_field *field_level = slog_new_field(ctx, SLOG_LEVEL, NULL);
	field_level->value.level = level;

	va_list args;
	va_start(args, msg);
	struct slog_field *element = NULL;
	while ((element = va_arg(args, struct slog_field *)),
	       element != SLOG_NULL) {
		slog_list_append(&field_message->link, &element->link);
	}
	va_end(args);

	struct slog_field *group = slog_new_group(
		ctx, NULL, field_time, field_level, field_message, SLOG_NULL);
	slog_list_append(&head, &group->link);
	struct slog_field *field_null = slog_new_field(ctx, SLOG_NULL, NULL);
	slog_list_append(&head, &field_null->link);

	struct slog_field *tmp;
	slog_list_for_each(tmp, &head, link) { slog_fprintf_field(tmp); }
}

#define SLOG(LEVEL, MSG, ...)                                                  \
	do {                                                                   \
		if (!SLOG_OUTPUT) {                                            \
			break;                                                 \
		}                                                              \
		if (LEVEL < SLOG_TRACE) {                                      \
			break;                                                 \
		}                                                              \
		struct slog_ctx SLOG_CTX_VAR = {0};                            \
		SLOG_CTX_VAR.capacity = 32;                                    \
		SLOG_CTX_VAR.pool = calloc(32, sizeof(struct slog_field));     \
		slog_main(&SLOG_CTX_VAR, LEVEL, MSG, ##__VA_ARGS__,            \
			  SLOG_NULL);                                          \
		free(SLOG_CTX_VAR.pool);                                       \
	} while (0)

#define SLOG_TRACE(msg, ...) SLOG(SLOG_TRACE, msg, ##__VA_ARGS__)
#define SLOG_DEBUG(msg, ...) SLOG(SLOG_DEBUG, msg, ##__VA_ARGS__)
#define SLOG_INFO(msg, ...) SLOG(SLOG_INFO, msg, ##__VA_ARGS__)
#define SLOG_WARN(msg, ...) SLOG(SLOG_WARN, msg, ##__VA_ARGS__)
#define SLOG_ERROR(msg, ...) SLOG(SLOG_ERROR, msg, ##__VA_ARGS__)
#define SLOG_FATAL(msg, ...) SLOG(SLOG_FATAL, msg, ##__VA_ARGS__)

#endif // SLOG_H
