#pragma once

void thread_pool_init();

typedef void(*thread_process_t)(void *input_data);

struct mutex_t;

mutex_t *request_mutex();

bool wait_for_mutex_and_own(
    mutex_t *mutex);

void release_mutex(
    mutex_t *mutex);

void request_thread_for_process(
    thread_process_t process,
    void *input_data);
