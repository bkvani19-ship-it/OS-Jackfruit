#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "monitor_ioctl.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s run <id> <program>\n", argv[0]);
        printf("  %s ps\n", argv[0]);
        printf("  %s stop <container_id>\n", argv[0]);
        return 1;
    }

    // ==========================
    // 🔥 ENGINE PS COMMAND
    // ==========================
    if (strcmp(argv[1], "ps") == 0) {
        printf("ID\tPID\tSTATUS\n");

        FILE *fp = fopen("containers.txt", "r");
        if (fp == NULL) {
            printf("No containers found\n");
            return 0;
        }

        char id[32];
        int pid;

        while (fscanf(fp, "%s %d", id, &pid) != EOF) {
            printf("%s\t%d\trunning\n", id, pid);
        }

        fclose(fp);
        return 0;
    }

    // ==========================
    // 🔥 ENGINE STOP COMMAND
    // ==========================
    if (strcmp(argv[1], "stop") == 0) {

        if (argc < 3) {
            printf("Usage: %s stop <container_id>\n", argv[0]);
            return 1;
        }

        FILE *fp = fopen("containers.txt", "r");
        if (fp == NULL) {
            printf("No containers found\n");
            return 1;
        }

        FILE *temp = fopen("temp.txt", "w");

        char id[32];
        int pid;
        int found = 0;

        while (fscanf(fp, "%s %d", id, &pid) != EOF) {

            if (strcmp(id, argv[2]) == 0) {
                kill(pid, SIGKILL);

                printf("Stopped container %s (PID %d)\n", id, pid);

                // 🔥 LOGGING
                FILE *log = fopen("engine.log", "a");
                fprintf(log, "Stopped %s PID %d\n", id, pid);
                fclose(log);

                found = 1;
            } else {
                fprintf(temp, "%s %d\n", id, pid);
            }
        }

        fclose(fp);
        fclose(temp);

        remove("containers.txt");
        rename("temp.txt", "containers.txt");

        if (!found) {
            printf("Container not found!\n");
        }

        return 0;
    }

    // ==========================
    // 🔥 RUN COMMAND
    // ==========================
    if (strcmp(argv[1], "run") == 0) {

        if (argc < 4) {
            printf("Usage: %s run <id> <program>\n", argv[0]);
            return 1;
        }

        int fd = open("/dev/container_monitor", O_RDWR);
        if (fd < 0) {
            perror("Failed to open device");
            return 1;
        }

        printf("Connected to kernel module!\n");

        pid_t pid = fork();

        if (pid == 0) {
            execvp(argv[3], &argv[3]);
            perror("exec failed");
        }
        else if (pid > 0) {

            struct monitor_request req;

            strcpy(req.container_id, argv[2]);
            req.pid = pid;
            req.soft_limit_bytes = 50 * 1024 * 1024;
            req.hard_limit_bytes = 100 * 1024 * 1024;

            if (ioctl(fd, MONITOR_REGISTER, &req) < 0) {
                perror("ioctl failed");
            } else {
                printf("Container registered in kernel!\n");
            }

            // 🔥 SAVE TO FILE
            FILE *fp = fopen("containers.txt", "a");
            if (fp != NULL) {
                fprintf(fp, "%s %d\n", argv[2], pid);
                fclose(fp);
            }

            // 🔥 LOGGING
            FILE *log = fopen("engine.log", "a");
            fprintf(log, "Started %s PID %d\n", argv[2], pid);
            fclose(log);
        }
        else {
            perror("fork failed");
        }
    }

    return 0;
}
