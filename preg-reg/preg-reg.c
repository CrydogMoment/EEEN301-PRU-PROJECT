/**
* @file preg-reg.c
* @author Riley&Zuni
* @date 7 April 2026
* @version 0.1
* @brief A Linux user space program that communicates with the rootkit.c LKM. It passes a
* string to the LKM and reads the response from the LKM. For this to work the device
* must be called /dev/rootkit.
*/

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<stdint.h>
#include<poll.h>
#include<assert.h>
#define BUFFER_LENGTH 256 ///< The buffer length (crude but fine)

static uint32_t receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM
void initialisePRU();
void stopPRU();
void startPRU();
void uploadRootkit();
int runBefore = 0;

int main(int argc, char *argv[]){
    int sample_count;
    if (argc != 2) {
        printf("Number of sensors please\n");
        return errno;
    }
    //uploads the LKM
    uploadRootkit();
    initialisePRU();

    int ret, fd;
    printf("Starting device...\n");
    fd = open("/dev/rootkit", O_RDWR); // Open the device with read/write access
    if (fd < 0){
        perror("Failed to open the device...");
        return errno;
    }

    while (1){
        printf("Enter the number of samples:");
        scanf("%d", &sample_count);

        if (argc != 2) {
            printf("Number of sensors argument pwease");
            return errno;
        }

        char message[10];
        sprintf(message, "%d", sample_count);
        if (sample_count <= 0 || sample_count > 100) {
            printf("Sample count out of range\n");
            return errno;
        }

        printf("Writing message to the device [%s].\n", message);
        ret = write(fd, message, strlen(message)); // Send the string to the LKM
        if (ret < 0){
            perror("Failed to write the message to the device.");
            return errno;
        }

        //starts the PRU, though I'm not sure this is where it should go.
        startPRU();

        printf("Reading from the device...\n");
        int i, n;
        short revents;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        i = poll(&pfd, 1, sample_count * 100);
        if (i == -1) {
            perror("poll");
            assert(0);
        }
        revents = pfd.revents;
        if (revents & POLLIN) {
            n = read(pfd.fd, receive, sample_count);
            printf("array:\n");
            double sum = 0.0;
            for (int i = 0; i < sample_count; i ++) {
                uint32_t num = receive[i];

                double scaled_down = num / 10000000.0;
                sum += scaled_down;

                printf("\t%.4fcm\n", scaled_down);
            }
            printf("\nAverage:\n\t%.4fcm\n", sum / sample_count);
        } else {
            printf("Poll failed\n");
        }

        printf("End of the program\n");
    }
    return 0;
}

void uploadRootkit(){
    FILE *pipe = popen("cd ../LKM && sh remove-rootkit.sh && sh upload-rootkit.sh", "r");
    if (pipe) { pclose(pipe); } else { perror("Failed to upload rootkit"); }
}

void initialisePRU(){
    FILE *pipe = popen("cd ../pru && sh compile_script.sh", "r");
    if (pipe) { pclose(pipe); } else { perror("Failed to initialize PRU"); }
}

void stopPRU(){
    FILE *pipe = popen("cd ../pru && sh stop_pru.sh", "r");
    if (pipe) { pclose(pipe); } else { perror("Failed to stop PRU"); }
}

void startPRU(){
    // TODO... do we really have to upload firmware EVERY time?
    // FILE *pipe;
    // if (runBefore) { pipe = popen("cd ../pru && sh start_pru.sh", "r"); }
    // else { pipe = popen("cd ../pru && sh upload_firmware.sh", "r"); }

    FILE *pipe = popen("cd ../pru && sh upload_firmware.sh", "r");
    if (pipe) { pclose(pipe); } else { perror("Failed to start PRU"); }
    runBefore = 1; // compiler will totally unroll this, right?
}
