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

#define RESET   "\033[0m"
#define MAGENTA "\033[1;35m"

#define MAX_VALUE 360

static uint32_t receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM
void initialisePRU();
void stopPRU();
void startPRU();
void uploadRootkit();
void displayForSensor();
char* getTextColor(float);
int runBefore = 0;
char textColor[23];

int main(int argc, char *argv[]){
    int sample_count;
    
    if (argc != 2) {
        printf("Number of sensors please\n");
        return errno;
    }

    int sensor_count = atoi(argv[1]);
    if (sensor_count > 3 || sensor_count < 1) {
        perror("Invalid sensor count.\n");
        return errno;
    }
    
    //uploads the LKM
    uploadRootkit();
    initialisePRU();

    int ret, fd;
    printf("Starting device...\n");
    fd = open("/dev/rootkit", O_RDWR); // Open the device with read/write access
    if (fd < 0){
        perror("Failed to open the device...\n");
        return errno;
    }
    char message[10];
    sprintf(message, "%d", sensor_count);
    if (write(fd, message, strlen(message)) < 0) {
        perror("Failed to write sensor count to the device.\n");
        return errno;
    }

    while (1){
        printf("Enter the number of samples:");

        if (scanf("%d", &sample_count) != 1) {
            perror("That's not a number, silly\n");
            return errno;
        }
        if (sample_count <= 0 || sample_count > 800 / sensor_count) {
            perror("Sample count out of range\n");
            continue;
        }

        sprintf(message, "%d", sample_count);
        printf("Writing message to the device [%s].\n", message);
        if (write(fd, message, strlen(message)) < 0){
            perror("Failed to write sample count to the device.\n");
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
            perror("Error polling device.\n");
            assert(0);
        }
        revents = pfd.revents;
        if (revents & POLLIN) {
            n = read(pfd.fd, receive, sample_count * sensor_count);
            for (int i = 0; i < sensor_count; i ++) {
                displayForSensor(sensor_count, sample_count, i);
            }
        } else {
            perror("Poll failed!\n");
        }

        printf("End of the program\n");
    }
    return 0;
}

void displayForSensor(int sensor_count, int sample_count, int n) {
    printf(MAGENTA"\nSensor %d:\n"RESET, n + 1);
    double sum = 0.0;
    for (int i = 0; i < sample_count ; i ++) {
        uint32_t num = receive[(i * sensor_count) + n];

        double scaled_down = num / 10000000.0; 
        sum += scaled_down;

        printf("\t%s%.4fcm\t", getTextColor(scaled_down),scaled_down);
        for (int i = 0; i < scaled_down; i += 3) {
            printf("█");
        }
        printf(RESET "\n");
    }
    printf(MAGENTA "Average:\n"RESET"\t%s%.4fcm\n"RESET, getTextColor(sum / sample_count), sum / sample_count);
}

void uploadRootkit(){
    char buffer[128];
    // Change directory to ../LKM/
    FILE *pipe = popen("cd ../LKM && sh remove-rootkit.sh && sh upload-rootkit.sh", "r");

    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) { }
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
}

void initialisePRU(){
    char buffer[128];
    // Change directory to ../pru/
    FILE *pipe = popen("cd ../pru && sh compile_script.sh", "r");

    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {}
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
}

void stopPRU(){
    char buffer[128];
    // Change directory to ../pru/
    FILE *pipe = popen("cd ../pru && sh stop_pru.sh", "r");

    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {}
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
}

void startPRU(){
    char buffer[128];
    FILE *pipe;
    pipe = popen("cd ../pru && sh upload_firmware.sh", "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {}
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
    runBefore = 1;
}

char* getTextColor(float value){
    uint8_t r, g, b;
    if(value < 60){
        r = 255 - (value / 60.0 * 240);
        g = (value / 60.0 * 240) + 15;
        b = 15;
    } else if (value < 180) {
        r = 15;
        g = 255 - ((value - 60.0) / 120.0 * 240);
        b = ((value - 60.0) / 120.0 * 240)+ 15;
    } else if (value < 360) {
        r = ((value - 180.0) / 180.0 * 240)+ 15;
        g = 15;
        b = 255;
    } else {
        r = 255;
        g = 15;
        b = 255;
    }
    sprintf(textColor, "\033[38;2;%d;%d;%dm", r, g, b);
    return textColor;
} 
