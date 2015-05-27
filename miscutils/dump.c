//#include "busybox.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define PARTITION_BLOCK_LEN 1024
#define TRANSFERT_BLOCK_LEN 1024*1024
#define MAX_LEN 255
#define BLOCKS_OFFSET 14
#define SKIP_LINES 2

char* hsize(double blocks, char *buf);
int dump_main(int argc, char *argv[]);

char* hsize(double blocks, char *buf) {
    int i = 0;
    double size = blocks*1024;
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (size > 1024) {
        size /= 1024;
        i++;
    }
    sprintf(buf, "%.*f %s", i, size, units[i]);
    return buf;
}

int dump_main(int argc, char *argv[])
{
    FILE * fp = NULL;
    int fd_r=NULL, fd_w=NULL;
    unsigned long long blocks;
    unsigned long long total_read, total_len, dn;
    int n, nw;
    char part[MAX_LEN] = {0};
    char size[MAX_LEN] = {0};
    char path[MAX_LEN] = {0};
    char path_output[MAX_LEN] = {0};
    char eta[MAX_LEN] = {0};
    int tp, hours, minutes, seconds;
    char copy_buffer[TRANSFERT_BLOCK_LEN] = {0};
    char *device = NULL;
    char *image = NULL;
    char buffer[MAX_LEN];
    double p = 0;
    int i = 0;
    char choice;
    time_t begin, current;

    if (argc != 3) {
        printf("Usage: dump DEVICE IMAGE_NAME\n");
        printf("Example: dump sda image_backup\n");
        printf("Version 3\n");
        goto error;
    }

    device = (char *) calloc(strlen(argv[1])+1, sizeof(char));
    if (device == NULL) {
        perror("Alloc device");
        goto error;
    }
    image = (char *) calloc(strlen(argv[2])+1, sizeof(char));
    if (image == NULL) {
        perror("Alloc image");
        goto error;
    }
    strcpy(device, argv[1]);
    strcpy(image, argv[2]);

    printf("DUMP /dev/%s\n", device);

    fp = fopen("/proc/partitions", "r");
    if (fp == NULL) {
        perror("Unable to open /proc/partitions");
        goto error;
    }

    while(fgets(buffer, MAX_LEN, fp) != NULL) {

        i += 1;

        if (i <= SKIP_LINES) {
            continue;
        }
        sscanf(buffer+BLOCKS_OFFSET, "%10llu %s", &blocks, (char *)&part);
        if (strncmp(device, part, strlen(device)) == 0) {

            strcpy(path, "/dev");
            strcat(path, "/");
            strcat(path, part);

            strcpy(path_output, image);
            strcat(path_output, ".");
            strcat(path_output, part);

            do {

                if (strlen(part) > strlen(device)) {
                    printf("\nDump %s  blocks=%llu size=%s ? (y/n): ", path, blocks, hsize(blocks, size));
                    total_len = blocks*PARTITION_BLOCK_LEN;
                } else {
                    printf("\nDump /dev/%s MBR size=512*2048 B ? (y/n): ", part);
                    strcat(path_output, ".mbr");
                    total_len = 512*2048;
                }
                choice = getchar();
                getchar(); //line return
            }
            while (choice != 'n' && choice != 'y');

            if (choice == 'y') {

                fd_r = open(path, O_RDONLY);
                if (fd_r < 0) {
                    perror("Unable to open read source partition");
                    goto error;
                }

                fd_w = open(path_output, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                if (fd_w < 0) {
                    perror("Unable to open destination file");
                    goto error;
                }

                dn = MIN(TRANSFERT_BLOCK_LEN, total_len);
                total_read = 0;
                begin = time(NULL);
                while(total_read < total_len) {
                    current = time(NULL);

                    p = ((double) total_read/(double) total_len);
                    current = time(NULL);
                    if (p) {
                        tp = difftime(current, begin);
                        seconds = (tp/p)-p-tp;
                        hours =  seconds / (60.0 * 60.0);
                        seconds -= hours * (60.0 * 60.0);
                        minutes = seconds / 60.0;
                        seconds -= minutes * 60.0;
                        sprintf(eta, "%02i:%02i:%02i", hours, minutes, seconds);
                    } else {
                        sprintf(eta, "N/A");
                    }

                    if (p != 0) {
                        printf("%.2f %% ETA %s\r", 100.0*p, eta);
                        fflush(stdout);
                    }

                    n = read(fd_r, copy_buffer, dn);
                    if (n < 0) {
                        fprintf(stderr, "bad sector, skipping\n");
                        lseek(fd_r, dn, SEEK_CUR);
                        memset(copy_buffer, '\0', dn);
                        nw = write(fd_w, copy_buffer, dn);
                        total_read += dn;
                        continue;
                    }
                    else if (n == 0) {
                        break;
                    }
                    else {
                        nw = write(fd_w, copy_buffer, n);
                    }

                    total_read += n;
                }

                if (fd_r)
                    close(fd_r);
                if (fd_w) {
                    fsync(fd_w);
                    close(fd_w);
                }
            }
        }
    }

    if (fp)
        fclose(fp);
    printf("\n");
    return 0;

error:
    if (device != NULL)
        free(device);
    if (image != NULL)
        free(image);
    if (fp != NULL)
        fclose(fp);
    if (fd_r)
        close(fd_r);
    if (fd_w)
        close(fd_w);
    return 1;
}
