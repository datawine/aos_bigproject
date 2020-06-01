#include "helper.h"

redisContext* init_redis(const char* hostname, int port) {
    redisContext* c;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            redisFree(c);
        } else {
        }
        return NULL;        
    } else {
    }
    return c;
}

void set_key(uint32_t key, uint32_t val, redisContext *c) {
    redisReply *reply;
    reply = redisCommand(c, "SET %u %u", key, val);
    freeReplyObject(reply);
}

uint8_t set_key_with_lock(uint32_t key, uint32_t val, redisContext* c) {
    redisReply *reply;
    uint32_t result;

    reply = redisCommand(c, "WATCH %u", key);
    freeReplyObject(reply);

    reply = redisCommand(c, "MULTI");
    freeReplyObject(reply);

    reply = redisCommand(c, "SET %u %u", key, val);
    freeReplyObject(reply);

    reply = redisCommand(c, "EXEC");
    result = (int)reply->elements;
    freeReplyObject(reply);

    if (result == 0)
        return 0; // set false, entry exist
    else
        return 1; // set true, entry created
}

void add_val_with_lock(uint32_t key, uint32_t val_to_add, redisContext *c) {
    redisReply *reply;
    uint8_t result;
    uint32_t tmp_val;

    do {
        reply = redisCommand(c, "WATCH %u", key);
        freeReplyObject(reply);

        reply = redisCommand(c, "GET %u", key);
        tmp_val = atoi(reply->str);
        freeReplyObject(reply);

        reply = redisCommand(c, "MULTI");
        freeReplyObject(reply);

        reply = redisCommand(c, "SET %u %u", key, tmp_val + val_to_add);
        freeReplyObject(reply);

        reply = redisCommand(c, "EXEC");
        result = (int)reply->elements;
        freeReplyObject(reply);
    } while (result == 0);
}

int get_val(uint32_t key, redisContext *c) {
    int val;
    redisReply *reply;
    reply = redisCommand(c, "GET %u", key);
    if (reply->type == REDIS_REPLY_NIL) {
        val = -1;
    } else
        val = atoi(reply->str);
    freeReplyObject(reply);
    return (int)val;
}

long get_long_val(uint32_t key, redisContext *c) {
    uint32_t val;
    redisReply *reply;
    reply = redisCommand(c, "GET %u", key);
    if (reply->type == REDIS_REPLY_NIL) {
        val = -1;
    } else
        val = atoi(reply->str);
    freeReplyObject(reply);
    return (long)val;
}

void set_str_key(char* key, char* val, redisContext *c) {
    redisReply *reply;
    reply = redisCommand(c, "SET %s %s", key, val);
    freeReplyObject(reply);
}

uint8_t get_str_val(char* key, char* val, int size, redisContext *c) {
    uint8_t result;
    redisReply *reply;
    reply = redisCommand(c, "GET %s", key);
    if (reply->type == REDIS_REPLY_NIL) {
        result = 0;
    } else {
        result = 1;
        strncpy(val, reply->str, size);
    }
    freeReplyObject(reply);
    return result;
}

uint8_t set_struct_with_lock(uint32_t key, char* val, redisContext *c) {
    redisReply *reply;
    uint32_t result;

    reply = redisCommand(c, "WATCH %u", key);
    freeReplyObject(reply);

    reply = redisCommand(c, "MULTI");
    freeReplyObject(reply);

    reply = redisCommand(c, "SET %u %s", key, val);
    freeReplyObject(reply);

    reply = redisCommand(c, "EXEC");
    result = (int)reply->elements;
    freeReplyObject(reply);

    if (result == 0)
        return 0; // set false, entry exist
    else
        return 1; // set true, entry created
}

uint8_t get_struct_val(uint32_t key, char* val, int size, redisContext *c) {
    uint8_t result;
    redisReply *reply;
    reply = redisCommand(c, "GET %u", key);
    if (reply->type == REDIS_REPLY_NIL) {
        result = 0;
    } else {
        result = 1;
        strncpy(val, reply->str, size);
    }
    freeReplyObject(reply);
    return result;
}

void flush_all(redisContext *c) {
    redisReply *reply;

    reply = redisCommand(c, "FLUSHALL");
    freeReplyObject(reply );
}