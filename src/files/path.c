
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "files/path.h"
#include "memory/kalloc.h"

void inlineReducePath(char* path) {
    int read_pos = 0;
    int insert_pos = 0;
    int segments = 0; // Holds the number of reducable segments e.g: /../seg1/seg2/ ('..' is not reducable)
    bool is_absolute = false;
    if (path[read_pos] == '/') {
        read_pos++;
        insert_pos++;
        is_absolute = true;
    }
    while (path[read_pos] != 0) {
        // Skip unnecessary '/' characters
        while (path[read_pos] == '/') {
            read_pos++;
        }
        // Find end of segment
        int next_slash = read_pos;
        while (path[next_slash] != 0 && path[next_slash] != '/') {
            next_slash++;
        }
        if (next_slash == read_pos + 1 && path[read_pos] == '.') {
            // 'seg1/./' is equal to 'seg1/' 
            read_pos += 2;
        } else if (next_slash == read_pos + 2 && path[read_pos] == '.' && path[read_pos + 1] == '.') {
            if (segments > 0) {
                // Remove the last segment e.g. '/seg1/seg2' -> '/seg1'
                insert_pos--;
                while(insert_pos > 0 && path[insert_pos - 1] != '/') {
                    insert_pos--;
                }
                read_pos += 3;
                segments--;
            } else {
                if (is_absolute) {
                    // '/../' is equal to '/'
                    read_pos += 3;
                } else {
                    // Write the segment (but don't increment the number of reducable segments)
                    while (read_pos <= next_slash && path[read_pos] != 0) {
                        path[insert_pos] = path[read_pos];
                        read_pos++;
                        insert_pos++;
                    }
                }
            }
        } else {
            // Write the segment
            while (read_pos <= next_slash && path[read_pos] != 0) {
                path[insert_pos] = path[read_pos];
                read_pos++;
                insert_pos++;
            }
            segments++;
        }
    }
    // Remove any trailing '/'
    if (insert_pos > 1 && path[insert_pos - 1] == '/') {
        insert_pos--;
    }
    path[insert_pos] = 0;
}

char* reducedPathCopy(const char* path) {
    if (path != NULL) {
        char* new_path = stringClone(path);
        inlineReducePath(new_path);
        return new_path;
    } else {
        return NULL;
    }
}

char* stringClone(const char* path) {
    if (path != NULL) {
        size_t path_length = strlen(path);
        char* new_path = kalloc(path_length + 1);
        memcpy(new_path, path, path_length + 1);
        return new_path;
    } else {
        return NULL;
    }
}

