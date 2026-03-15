#include "sensor_sim.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static pthread_t sensor_thread;
static int running = 0;
static msg_queue_t *msg_q = NULL;

static void *sensor_thread_func(void *arg)
{
	(void)arg;
	char buf[MAX_MSG_LEN];
	float temp, humidity, voltage;

	srand(time(NULL));

	while (running) {
		// 生成模拟数据
		temp = 20.0f + (float)(rand() % 150) / 10.0f;     // 20.0 - 35.0
		humidity = 40.0f + (float)(rand() % 400) / 10.0f; // 40.0 - 80.0
		voltage = 3.0f + (float)(rand() % 60) / 100.0f;   // 3.00 - 3.60

		snprintf(buf, sizeof(buf),
			"{\"type\":\"sensor\",\"data\":{\"temp\":%.1f,\"humidity\":%.1f,\"voltage\":%.2f}}",
			temp, humidity, voltage);

		msg_queue_push(msg_q, buf);

		// 每 2 秒采集一次
		sleep(2);
	}

	return NULL;
}

int sensor_sim_start(msg_queue_t *queue)
{
	if (running)
		return 0;

	msg_q = queue;
	running = 1;

	if (pthread_create(&sensor_thread, NULL, sensor_thread_func, NULL) != 0) {
		running = 0;
		return -1;
	}

	return 0;
}

void sensor_sim_stop(void)
{
	if (!running)
		return;

	running = 0;
	pthread_join(sensor_thread, NULL);
}
