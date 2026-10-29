// SPDX-License-Identifier: LGPL-2.1-or-later
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_identity(const char *block, const char *field,
                         char *buffer, size_t size) {
    char path[512];
    snprintf(path, sizeof(path), "/sys/class/block/%s/device/%s", block, field);
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    if (!fgets(buffer, size, file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    size_t length = strlen(buffer);
    while (length && isspace((unsigned char)buffer[length - 1]))
        buffer[--length] = '\0';
    return 1;
}

int main(void) {
    DIR *directory = opendir("/sys/class/block");
    if (!directory) {
        perror("ipod-time-sync-safe: /sys/class/block");
        return 1;
    }

    struct dirent *entry;
    char device[512] = "";
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "sd", 2) != 0) continue;
        char partition_path[512];
        snprintf(partition_path, sizeof(partition_path),
                 "/sys/class/block/%s/partition", entry->d_name);
        if (access(partition_path, F_OK) == 0) continue;

        char vendor[128], model[128];
        if (!read_identity(entry->d_name, "vendor", vendor, sizeof(vendor)) ||
            !read_identity(entry->d_name, "model", model, sizeof(model)))
            continue;
        if (strcmp(vendor, "Apple") == 0 && strcmp(model, "iPod") == 0) {
            snprintf(device, sizeof(device), "/dev/%s", entry->d_name);
            break;
        }
    }
    closedir(directory);

    if (!device[0]) {
        fprintf(stderr, "ipod-time-sync-safe: no Apple iPod disk found\n");
        return 1;
    }
    if (setuid(0) != 0) {
        perror("ipod-time-sync-safe: setuid");
        return 1;
    }
    /* A second exec after setuid(0) may no longer receive the dynamic
       loader's secure-execution treatment. Never pass caller-controlled
       loader or library environment variables into the root process. */
    if (clearenv() != 0 ||
        setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1) != 0) {
        perror("ipod-time-sync-safe: sanitize environment");
        return 1;
    }
    execl("/usr/bin/ipod-time-sync", "ipod-time-sync", device, (char *)NULL);
    fprintf(stderr, "ipod-time-sync-safe: exec failed: %s\n", strerror(errno));
    return 1;
}
