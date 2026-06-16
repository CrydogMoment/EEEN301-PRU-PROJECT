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
#define BUFFER_LENGTH 800 ///< The buffer length (crude but fine)

#define RESET   "\033[0m"
#define MAGENTA "\033[1;35m"

#define GRAPH_HEIGHT 36

static uint32_t receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM
void initialisePRU();
void stopPRU();
void startPRU();
void uploadRootkit();
void displayForSensor();
char* magicRGB(uint32_t);
char* getTextColor(float);
void coolGraph(int, int);
int runBefore = 0;
char textColor[23];
static uint32_t graph[800][GRAPH_HEIGHT];
static int max_value;

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
            
            // init pixel graph
            for (int i = 0; i < sample_count; i ++) { // clear it lol
                for (int j = 0; j < GRAPH_HEIGHT; j ++) {
                    graph[i][j] = 0;
                }
            }
            max_value = 0;

            for (int i = 0; i < sensor_count; i ++) {
                displayForSensor(sensor_count, sample_count, i);
            }

            printf( MAGENTA "\nREALLY COOL GRAPHS:\nCombined sensors:\n" RESET);
            for (int i = (max_value/ (360 / GRAPH_HEIGHT)) - 1; i >= 0; i --) {
                for (int j = 0; j < sample_count; j ++) {
                    printf("%s█" RESET, magicRGB(graph[j][i]));
                }
                printf("\n");
            }
            if (sensor_count > 1) {
                for (int i = 0; i < sensor_count; i ++) {
                    coolGraph(i, sample_count);
                }
            }
        } else {
            perror("Poll failed!\n");
        }

        printf("End of the program\n");
    }
    return 0;
}

void coolGraph(int sensor_n, int sample_conut) {
    printf( MAGENTA "\nSensor %d:\n" RESET, sensor_n + 1);
    for (int i = (max_value/ (360 / GRAPH_HEIGHT)) - 1; i >= 0; i --) {
        for (int j = 0; j < sample_conut; j ++) {
            printf("%s█" RESET, magicRGB(graph[j][i] & (0xff << (sensor_n * 8))));
        }
        printf("\n");
    }
}

void displayForSensor(int sensor_count, int sample_count, int sensor_n) {
    printf(MAGENTA"\nSensor %d:\n"RESET, sensor_n + 1);
    double sum = 0.0;
    for (int i = 0; i < sample_count ; i ++) {
        uint32_t num = receive[(i * sensor_count) + sensor_n];

        double scaled_down = num / 10000000.0; 
        sum += scaled_down;

        if (scaled_down > max_value) { max_value = scaled_down; }

        for (int j = 0; j < GRAPH_HEIGHT; j ++) {
            if (j * (360 / GRAPH_HEIGHT) <= scaled_down) {
                graph[i][j] = graph[i][j] | (0xff << (sensor_n * 8));
            }
        }

        printf("\t%s%.4fcm\t", getTextColor(scaled_down),scaled_down);
        for (int j = 0; j < scaled_down; j += 3) {
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

char* magicRGB(uint32_t pixel) {
    uint8_t r = pixel & 0xff;
    uint8_t g = (pixel & 0xff00) >> 8;
    uint8_t b = (pixel & 0xff0000) >> 16;
    sprintf(textColor, "\033[38;2;%d;%d;%dm", r, g, b);
    return textColor;
}

char* getTextColor(float value) {
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
