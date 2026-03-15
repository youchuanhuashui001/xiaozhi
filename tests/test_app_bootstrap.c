#include <assert.h>

#include "app.h"

int main(void)
{
	app_runtime_t app;
	app_options_t opts = {
		.config_path = "config/xiaozhi.ini.example",
		.check_only = 1
	};

	assert(app_init(&app, &opts) == 0);
	app_destroy(&app);
	return 0;
}
