#ifndef BLUEWAKE_BMG_TEXT_H
#define BLUEWAKE_BMG_TEXT_H

// Translated messages for The Wind Waker's message file (MESGbmg1, the US disc's
// zel_00.bmg). The translation is a text file the app reads from its own data folder
// (es.txt in the app data folder); the disc and the game code are not changed. Each line is
//
//   <offset><TAB><text>
//
// where <offset> is the message's text offset in the original file, \n in the text
// is a line break, and [n] is the message's nth format tag (a 0x1A tag of the
// original message, copied byte for byte). Lines starting with # are comments.
// The text is Latin-1, so accented letters are single bytes (the font has them).
//
// The file is rebuilt in place: each message keeps its place in the message table
// (the INF1 section), and the text pool is repacked with the translations. The
// total size of the file does not change.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BluewakeBmgTextLine {
    uint32_t offset;
    uint8_t* bytes;  // the text, without the terminating zero
    size_t length;
} BluewakeBmgTextLine;

static BluewakeBmgTextLine* g_bmg_text_lines;
static size_t g_bmg_text_count;
static bool g_bmg_text_loaded;
static char* g_bmg_text_path;

static uint32_t bmg_text_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t bmg_text_u16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void bmg_text_put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int bmg_text_compare(const void* a, const void* b) {
    const BluewakeBmgTextLine* x = a;
    const BluewakeBmgTextLine* y = b;
    return (x->offset > y->offset) - (x->offset < y->offset);
}

static uint32_t bmg_text_fnv1a(const uint8_t* p, size_t n) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

// Reads the translation file once. A missing or empty file leaves no translations.
static void bmg_text_load(void) {
    if (g_bmg_text_loaded)
        return;
    g_bmg_text_loaded = true;
    if (g_bmg_text_path == NULL)
        return;
    FILE* file = fopen(g_bmg_text_path, "rb");
    if (file == NULL)
        return;
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return;
    }
    uint8_t* buffer = malloc((size_t)size + 1);
    const size_t got = buffer != NULL ? fread(buffer, 1, (size_t)size, file) : 0;
    fclose(file);
    if (buffer == NULL)
        return;
    buffer[got] = 0;
    size_t lines = 1;
    for (size_t i = 0; i < got; i++)
        lines += buffer[i] == '\n';
    BluewakeBmgTextLine* table = calloc(lines, sizeof *table);
    if (table == NULL) {
        free(buffer);
        return;
    }
    size_t count = 0;
    size_t p = 0;
    while (p < got) {
        size_t end = p;
        while (end < got && buffer[end] != '\n')
            end++;
        size_t stop = end;
        if (stop > p && buffer[stop - 1] == '\r')
            stop--;
        if (stop > p && buffer[p] != '#') {
            size_t q = p;
            uint32_t offset = 0;
            bool digits = false;
            while (q < stop && buffer[q] >= '0' && buffer[q] <= '9') {
                offset = offset * 10u + (uint32_t)(buffer[q] - '0');
                digits = true;
                q++;
            }
            if (digits && q < stop && buffer[q] == '\t') {
                q++;
                uint8_t* text = malloc(stop - q + 1);
                if (text != NULL) {
                    size_t n = 0;
                    for (size_t r = q; r < stop; r++) {
                        if (buffer[r] == '\\' && r + 1 < stop && buffer[r + 1] == 'n') {
                            text[n++] = 0x0A;
                            r++;
                        } else {
                            text[n++] = buffer[r];
                        }
                    }
                    table[count].offset = offset;
                    table[count].bytes = text;
                    table[count].length = n;
                    count++;
                }
            }
        }
        p = end + 1;
    }
    free(buffer);
    qsort(table, count, sizeof *table, bmg_text_compare);
    g_bmg_text_lines = table;
    g_bmg_text_count = count;
}

static const BluewakeBmgTextLine* bmg_text_find(uint32_t offset) {
    size_t lo = 0, hi = g_bmg_text_count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (g_bmg_text_lines[mid].offset < offset)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < g_bmg_text_count && g_bmg_text_lines[lo].offset == offset)
        return &g_bmg_text_lines[lo];
    return NULL;
}

// The tags of an original message, in order, as offsets and lengths in the pool.
typedef struct BluewakeBmgTag {
    size_t start;
    size_t length;
} BluewakeBmgTag;

static size_t bmg_text_tags(const uint8_t* text, size_t length, BluewakeBmgTag* tags, size_t max) {
    size_t n = 0;
    size_t p = 0;
    while (p < length && text[p] != 0) {
        if (text[p] == 0x1A && p + 1 < length && text[p + 1] >= 2) {
            const size_t tag = text[p + 1];
            if (n < max && p + tag <= length) {
                tags[n].start = p;
                tags[n].length = tag;
                n++;
            }
            p += tag;
        } else {
            p++;
        }
    }
    return n;
}

// Writes the translated text, with its [n] markers replaced by the original tags.
// Returns false (and writes nothing useful) if the markers do not match the tags.
static bool bmg_text_compose(const uint8_t* original, size_t original_length, const BluewakeBmgTextLine* line,
                             uint8_t* out, size_t* out_length, size_t out_max) {
    BluewakeBmgTag tags[256];
    const size_t tag_count = bmg_text_tags(original, original_length, tags, 256);
    size_t n = 0;
    size_t used = 0;
    for (size_t i = 0; i < line->length;) {
        if (line->bytes[i] == '[') {
            size_t j = i + 1;
            size_t index = 0;
            bool digits = false;
            while (j < line->length && line->bytes[j] >= '0' && line->bytes[j] <= '9') {
                index = index * 10u + (size_t)(line->bytes[j] - '0');
                digits = true;
                j++;
            }
            if (digits && j < line->length && line->bytes[j] == ']') {
                if (index >= tag_count)
                    return false;
                if (n + tags[index].length > out_max)
                    return false;
                memcpy(out + n, original + tags[index].start, tags[index].length);
                n += tags[index].length;
                used++;
                i = j + 1;
                continue;
            }
        }
        if (n + 1 > out_max)
            return false;
        out[n++] = line->bytes[i++];
    }
    if (used != tag_count)
        return false;
    if (n + 1 > out_max)
        return false;
    out[n++] = 0;
    *out_length = n;
    return true;
}

typedef struct BluewakeBmgTextMap {
    uint32_t original;
    uint32_t moved;
} BluewakeBmgTextMap;

static int bmg_text_map_compare(const void* a, const void* b) {
    const BluewakeBmgTextMap* x = a;
    const BluewakeBmgTextMap* y = b;
    return (x->original > y->original) - (x->original < y->original);
}

// Rebuilds one loaded message file in place. Returns the number of translated
// messages, or -1 if the data is not the original English file (already
// translated, or some other file), in which case nothing is written.
static int bluewake_bmg_text_apply(uint8_t* data, size_t available) {
    bmg_text_load();
    if (g_bmg_text_count == 0 || available < 32 || memcmp(data, "MESGbmg1", 8) != 0)
        return -1;
    // The size field counts 32-byte blocks (as bmg_patch.h reads it).
    const size_t size = (size_t)bmg_text_u32(data + 8) * 32u;
    if (size < 32 || size > available)
        return -1;
    size_t at = 32, inf = 0, inf_size = 0, dat = 0, dat_size = 0;
    while (at + 8 <= size) {
        const size_t section = bmg_text_u32(data + at + 4);
        if (section < 8 || section > size - at)
            break;
        if (memcmp(data + at, "INF1", 4) == 0) {
            inf = at;
            inf_size = section;
        } else if (memcmp(data + at, "DAT1", 4) == 0) {
            dat = at;
            dat_size = section;
        }
        at += section;
    }
    if (inf == 0 || dat == 0 || inf_size < 16 || dat_size < 8)
        return -1;
    const size_t count = bmg_text_u16(data + inf + 8);
    const size_t entry = bmg_text_u16(data + inf + 10);
    if (entry < 4 || count == 0 || 16 + count * entry > inf_size)
        return -1;
    const size_t text_start = dat + 8;
    const size_t pool_size = dat_size - 8;
    if (text_start + pool_size > size)
        return -1;
    uint8_t* pool = data + text_start;
    // Only the original US message table (this fingerprint of its INF1 section) is
    // translated; a table already translated, or another file, is left alone.
    if (inf_size != 105888u || bmg_text_fnv1a(data + inf, inf_size) != 0x3EB42BCCu)
        return -1;

    // The distinct text offsets used by the message table, in ascending order.
    BluewakeBmgTextMap* map = calloc(count, sizeof *map);
    uint8_t* out = malloc(pool_size);
    if (map == NULL || out == NULL) {
        free(map);
        free(out);
        return -1;
    }
    size_t distinct = 0;
    for (size_t m = 0; m < count; m++) {
        map[distinct].original = bmg_text_u32(data + inf + 16 + m * entry);
        map[distinct].moved = 0;
        distinct++;
    }
    qsort(map, distinct, sizeof *map, bmg_text_map_compare);
    size_t unique = 0;
    for (size_t i = 0; i < distinct; i++)
        if (unique == 0 || map[unique - 1].original != map[i].original)
            map[unique++] = map[i];
    distinct = unique;

    // Every message keeps room for its original text: a translation is used only
    // if the messages after it still fit, so a long translation never breaks the file.
    size_t* original_size = calloc(distinct + 1, sizeof *original_size);
    if (original_size == NULL) {
        free(map);
        free(out);
        return -1;
    }
    for (size_t i = 0; i < distinct; i++) {
        const uint32_t original_offset = map[i].original;
        size_t length = 0;
        if (original_offset < pool_size) {
            const uint8_t* original = pool + original_offset;
            // A message ends at its terminating zero, but a tag can hold zeros of its own
            // (1A 06 FF 00 00 01): tags are skipped whole.
            while (original_offset + length < pool_size && original[length] != 0) {
                if (original[length] == 0x1A && original_offset + length + 1 < pool_size && original[length + 1] >= 2)
                    length += original[length + 1];
                else
                    length++;
            }
            if (original_offset + length > pool_size)
                length = pool_size - original_offset;
            original_size[i] = length + 1;
        } else {
            original_size[i] = 1;
        }
    }
    // rest[i]: the original size of every message from i on.
    size_t* rest = calloc(distinct + 1, sizeof *rest);
    if (rest == NULL) {
        free(original_size);
        free(map);
        free(out);
        return -1;
    }
    for (size_t i = distinct; i-- > 0;)
        rest[i] = rest[i + 1] + original_size[i];

    size_t written = 0;
    int translated = 0;
    for (size_t i = 0; i < distinct; i++) {
        const uint32_t original_offset = map[i].original;
        const uint8_t* original = original_offset < pool_size ? pool + original_offset : NULL;
        map[i].moved = (uint32_t)written;
        const BluewakeBmgTextLine* line = original != NULL ? bmg_text_find(original_offset) : NULL;
        if (line != NULL) {
            size_t length = 0;
            const size_t room = pool_size - written - rest[i + 1];
            if (room > 0 && bmg_text_compose(original, original_size[i] - 1, line, out + written, &length, room)) {
                translated++;
                written += length;
                continue;
            }
        }
        // The original message, with its terminating zero.
        const size_t copy = original_size[i];
        if (written + copy > pool_size) {
            translated = -1;
            break;
        }
        if (original != NULL && copy > 1)
            memcpy(out + written, original, copy - 1);
        out[written + copy - 1] = 0;
        written += copy;
    }
    if (translated >= 0 && written <= pool_size) {
        for (size_t m = 0; m < count; m++) {
            uint8_t* field = data + inf + 16 + m * entry;
            const uint32_t original_offset = bmg_text_u32(field);
            BluewakeBmgTextMap key = {original_offset, 0};
            const BluewakeBmgTextMap* found = bsearch(&key, map, distinct, sizeof *map, bmg_text_map_compare);
            if (found != NULL)
                bmg_text_put_u32(field, found->moved);
        }
        memcpy(pool, out, written);
        memset(pool + written, 0, pool_size - written);
    } else {
        translated = -1;
    }
    free(rest);
    free(original_size);
    free(map);
    free(out);
    return translated;
}

#endif
