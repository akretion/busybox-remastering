//#include "busybox.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define PARTITION_BLOCK_LEN 1024
#define TRANSFERT_BLOCK_LEN 1024*1024
#define MAX_LEN 255
#define DEFAULT_DEVICE "sda"
#define DEFAULT_IMAGE_DIR "/images"
#define DEFAULT_IMAGE "master"

int do_partprobe(char *device);
int do_restore(char *device, char *image_dir, char *image);
int reinstall_main(int argc, char *argv[]);

int do_partprobe(char *device) {
    int fd;
    char dev[MAX_LEN] = {0};
    strcpy(dev, "/dev/");
    strcat(dev, device);
    fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror("Unable to open device for partprobe");
        return -1;
    }
    ioctl(fd, BLKRRPART, NULL);
    close(fd);
    return 0;
}

int do_restore(char *device, char *image_dir, char *image) {

    int fd_r, fd_w;
    char path[MAX_LEN] = {0};
    char path_output[MAX_LEN] = {0};
    size_t total_read, total_len, dn;
    int n, nw;
    char eta[MAX_LEN] = {0};
    int tp, hours, minutes, seconds;
    char copy_buffer[TRANSFERT_BLOCK_LEN] = {0};
    double p = 0;
    time_t begin, current;

    strcpy(path, image_dir);
    strcat(path, "/");
    strcat(path, image);
    fd_r = open(path, O_RDONLY);
    if (fd_r < 0) {
        perror("Unable to open source file");
        return -1;
    }

    total_len = lseek(fd_r, 0, SEEK_END);
    lseek(fd_r, 0, SEEK_SET);

    strcpy(path_output, "/dev/");
    strcat(path_output, device);

    fd_w = open(path_output, O_WRONLY|O_TRUNC);
    if (fd_w < 0) {
        perror("Unable to open destination partition");
        return -1;
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

        printf("%.2f %% ETA %s\r", 100.0*p, eta);
        fflush(stdout);

        n = read(fd_r, copy_buffer, dn);
        if (n < 0) {
            perror("bad sector, skipping");
            lseek(fd_r, dn, SEEK_CUR);
            total_read += dn;
            memset(copy_buffer, '\0', dn);
            nw = write(fd_w, copy_buffer, dn);
            continue;
        }
        else if (n == 0) {
            break;
        }
        else {
            nw = write(fd_w, copy_buffer, n);
            if (nw != n) {
                perror("write error");
            }
        }

        total_read += n;

    }
    printf("REINSTALL %s DONE           \n", device);
    close(fd_r);
    close(fd_w);
    return 1;
}

int reinstall_main(int argc, char *argv[])
{
    DIR  *d;
    struct dirent *dir;
    int i=0;
    char *p = NULL;
    char *device = NULL;
    char *image_dir = NULL;
    char *image = NULL;
    char *image_full_name = NULL;
    char *image_mbr = NULL;

    if (argc != 1 && argc !=4) {
        printf("Usage: reinstall [DEVICE] [IMAGE_DIR] [IMAGE]\n");
        printf("Example: reinstall\n");
        printf("same as: reinstall %s %s %s\n", DEFAULT_DEVICE, DEFAULT_IMAGE_DIR, DEFAULT_IMAGE);
        goto error;
    }
    if (argc == 1) {
        device = strdup(DEFAULT_DEVICE);
        image_dir = strdup(DEFAULT_IMAGE_DIR);
        image = strdup(DEFAULT_IMAGE);
    }

    if (argc == 4) {
        device = strdup(argv[1]);
        image_dir = strdup(argv[2]);
        image = strdup(argv[3]);
    }


    image_full_name = (char *) calloc(MAX_LEN, sizeof(char));
    strcat(image_full_name, image_dir);
    strcat(image_full_name, "/");
    image_mbr = (char *) calloc(MAX_LEN, sizeof(char));
    strcat(image_mbr, image);
    strcat(image_mbr, ".");
    strcat(image_mbr, device);
    strcat(image_mbr, ".mbr");
    strcat(image_full_name, image_mbr);

    /* restore MBR */
    if( access(image_full_name, R_OK) != -1 ) {
        do_restore(device, image_dir, image_mbr);
        do_partprobe(device);
    }

    d = opendir(image_dir);
    if (!d) {
        fprintf(stderr, "Unable to open dir %s\n", image_dir);
        goto error;
    }

    while ((dir = readdir(d)) != NULL)
    {
        if (image_full_name)
            free(image_full_name);

        image_full_name = strdup(dir->d_name);
        if (strncmp(image, image_full_name, strlen(image)) == 0 &&
            strcmp(image_full_name, image_mbr) != 0
        ) {
            i = 0;
            p = image_full_name + strlen(image);

            while (*(p+i) && (*(p+i) == *(device+i) || *(device+i) == '.')) {
                i++;
            }

            do_restore(p+1, image_dir, image_full_name);
        }
    }

    free(image_full_name);
    closedir(d);
    printf("\n\nDONE! RESTART YOUR COMPUTER!\n");
    return 0;

error:
    free(image_full_name);
    return 1;

}
