// MT25087_Part_A_A1_common.h
#ifndef COMMON_H
#define COMMON_H

#define NUM_FIELDS 8

typedef struct {
    char *fields[NUM_FIELDS];
    int field_size;
} message_t;

#endif