#pragma once

#include <stdbool.h>

typedef struct {
    bool available;
    float x;
    float y;
    float z;
    float movement;
    float rotation;
    unsigned shake_count;
} aura_motion_status_t;

void aura_motion_init(void);
void aura_motion_get_status(aura_motion_status_t *status);
