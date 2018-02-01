#include "cuckoo.h"
#include <assert.h>
#include <stdio.h>
#include "hashutil.h"

static uint32_t lookup_entry(struct cuckoo_filter *filter, uint32_t bindex) {
    int bucket_size = filter->bucket_size;
    for (uint32_t i = 0; i < bucket_size; i++) {
        if (!filter->bucket[bindex * bucket_size + i])
            return bindex * bucket_size + i;
    }

    return CUCKOO_NEXIST;
}

static int lookup_entry_fp(struct cuckoo_filter *filter, int bindex,
                           uint8_t fp) {
    int bucket_size = filter->bucket_size;

    for (int i = 0; i < bucket_size; i++) {
        if (filter->bucket[bindex * bucket_size + i] == fp)
            return bindex * bucket_size + i;
    }

    return CUCKOO_NEXIST;
}

// a random function
int cuckoo_random(int max) { return rand() % max; }

struct cuckoo_filter *cuckoo_init(int poweroftwo, int bucket_size, int kicks,
                                  hash_func_t hash, fp_func_t fp) {
    struct cuckoo_filter *filter = malloc(sizeof(struct cuckoo_filter));
    filter->max_kicks = kicks;
    filter->counts = 0;
    filter->bucket_capacity = 1 << poweroftwo;
    filter->bucket_size = bucket_size;
    filter->poweroftwo = poweroftwo;
    filter->evicate = 0;
    filter->hash = hash;
    filter->fp = fp;
    filter->bucket =
        malloc(sizeof(uint8_t) * bucket_size * filter->bucket_capacity);
    memset(filter->bucket, 0x0, bucket_size * filter->bucket_capacity);

    return filter;
}

void cuckoo_deinit(struct cuckoo_filter *filter) {
    free(filter->bucket);
    free(filter);
}

static int x = 0;

int cuckoo_insert(struct cuckoo_filter *filter, const char *key, size_t len) {
    uint8_t fp = filter->fp(key, len);
    uint32_t mask = (1 << filter->poweroftwo) - 1;

    uint32_t i1 = filter->hash(key, len) & mask;
    uint32_t i2 = (filter->hash((char *)&fp, sizeof(uint8_t)) ^ i1) & mask;
    /*printf("insert %d %d %d %d %d\n", i1,*/
    /*filter->bucket[i1 * filter->bucket_size], i2,*/
    /*filter->bucket[i2 * filter->bucket_size], fp);*/
    uint32_t entry = 0;

    if ((entry = lookup_entry(filter, i1)) != CUCKOO_NEXIST) {
        filter->bucket[entry] = fp;
        filter->counts++;
        return CUCKOO_OK;
    } else if ((entry = lookup_entry(filter, i2)) != CUCKOO_NEXIST) {
        filter->bucket[entry] = fp;
        filter->counts++;
        return CUCKOO_OK;
    } else {
        uint32_t i = (cuckoo_random(2) == 0) ? i1 : i2;

        for (int n = 0; n < filter->max_kicks; n++) {
            entry =
                i * filter->bucket_size + cuckoo_random(filter->bucket_size);
            uint8_t pick = filter->bucket[entry];
            filter->bucket[entry] = fp;
            fp = pick;

            i = (filter->hash((char *)&fp, sizeof(uint8_t)) ^ i) & mask;
            if ((entry = lookup_entry(filter, i)) != CUCKOO_NEXIST) {
                filter->bucket[entry] = fp;
                /*printf("insert entry %d %d",i,fp);*/
                filter->counts++;
                return CUCKOO_OK;
            }
        }
    }

    filter->evicate++;
    // ensure no false negatives as long as bucket overflow never occurs
    return CUCKOO_ERR;
}

int cuckoo_lookup(struct cuckoo_filter *filter, const char *key, size_t len) {
    uint8_t fp = filter->fp(key, len);
    uint32_t mask = (1 << filter->poweroftwo) - 1;

    uint32_t i1 = filter->hash(key, len) & mask;
    uint32_t i2 = (filter->hash((char *)&fp, sizeof(uint8_t)) ^ i1) & mask;
    /*printf("lookup %d %d %d %d %d\n",i1,bucket[i1],i2,bucket[i2],fp);*/
    if (lookup_entry_fp(filter, i1, fp) != CUCKOO_NEXIST ||
        lookup_entry_fp(filter, i2, fp) != CUCKOO_NEXIST) {
        return CUCKOO_OK;
    }

    return CUCKOO_ERR;
}

int cuckoo_count(struct cuckoo_filter *filter) { return filter->counts; }

int cuckoo_delete(struct cuckoo_filter *filter, const char *key, size_t len) {
    uint8_t fp = filter->fp(key, len);
    uint32_t mask = (1 << filter->poweroftwo) - 1;
    uint32_t i1 = filter->hash(key, len) & mask;
    uint32_t i2 = (filter->hash((char *)&fp, sizeof(uint8_t)) ^ i1) & mask;
    uint32_t entry = 0;

    if ((entry = lookup_entry_fp(filter, i1, fp)) != CUCKOO_NEXIST) {
        filter->bucket[entry] = 0;
        filter->counts--;
        return CUCKOO_OK;
    }

    if ((entry = lookup_entry_fp(filter, i2, fp)) != CUCKOO_NEXIST) {
        filter->bucket[entry] = 0;
        filter->counts--;
        return CUCKOO_OK;
    }

    return CUCKOO_ERR;
}

// bucket capacity must be 2's pow
