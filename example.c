#include <stdbool.h>
#include <stdio.h>

#include "slog.h"

int main() {
	const char *name = "qaqland";
	const char *email = "qaq@qaq.land";
	unsigned int id = 233;
	float score = 95.5;
	bool is_active = true;

	SLOG_SET_OUTPUT(stdout);

	SLOG(SLOG_DEBUG, "single-string", S_STR("name", name));
	SLOG(SLOG_WARN, "string escape", S_STR("na\"me", "try \"scape"));
	SLOG_INFO("info helper", S_STR("name", name));

	SLOG_DEBUG("test group", S_STR("name", name),
		   S_GROUP("details", S_STR("check", "is_ok"),
			   S_GROUP("user", S_INT("id", id), S_STR("name", name),
				   S_STR("email", email))));

	SLOG(SLOG_ERROR, "all support types", S_STR("app", "MyApp"),
	     S_STR("email", email), S_INT("user_id", id),
	     S_FLOAT("score", score), S_BOOL("is_active", is_active));

	// FILE *f = fopen(".test", "w");
	// SLOG_SET_FILE(f);
	// SLOG(
	//     SLOG_STR("message", "Hello from stdout"),
	//     SLOG_INT("example", 123),
	//     SLOG_BOOL("success", true)
	// );
	// fclose(f);

	return 0;
}
