#ifndef SENSOR_SIM_H
#define SENSOR_SIM_H

#include "msg_queue.h"

/**
 * 启动模拟传感器采集线程
 * @param queue 消息队列指针，采集的数据将推送到此队列
 * @return 0 成功，-1 失败
 */
int sensor_sim_start(msg_queue_t *queue);

/**
 * 停止模拟传感器采集线程
 */
void sensor_sim_stop(void);

#endif /* SENSOR_SIM_H */
