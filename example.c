#include <stdbool.h>
#include <stdio.h>

#include "slog.h"

int main() {
	const char *name = "qaqland";
	const char *email = "qaq@qaq.land";
	unsigned int id = 233;
	float score = 95.5;
	bool is_active = true;

	SLOG(SLOG_DEBUG, "single-string", SLOG_STRING("name", name));
	SLOG(SLOG_WARN, "string escape", SLOG_STRING("na\"me", "try \"scape"));
	SLOG(SLOG_INFO, "info helper", SLOG_STRING("name", name));

	SLOG(SLOG_ERROR, "test group", SLOG_STRING("name", name),
	     SLOG_OBJECT("details", SLOG_BOOL("check", false),
			 SLOG_OBJECT("user", SLOG_INT("id", id),
				     SLOG_STRING("name", name),
				     SLOG_STRING("email", email))));

	SLOG(SLOG_ERROR, "all support types", SLOG_STRING("app", "MyApp"),
	     SLOG_STRING("email", email), SLOG_INT("user_id", id),
	     SLOG_FLOAT("score", score), SLOG_BOOL("is_active", is_active));

	SLOG(SLOG_INFO, "we have array now",
	     SLOG_ARRAY("brrby", SLOG_INT(NULL, 2), SLOG_INT(NULL, 1),
			SLOG_INT("striped key", 3)));

#define MYLOG(MSG, ...)                                                        \
	SLOG(SLOG_INFO, MSG, SLOG_INT("user_id", id), ##__VA_ARGS__)

	MYLOG("log from sub-logger");
#undef MYLOG

	SLOG_FREE();
	return 0;
}
