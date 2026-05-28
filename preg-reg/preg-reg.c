/**
* @file preg-reg.c
* @author Riley&Zuni
* @date 7 April 2026
* @version 0.1
* @brief A Linux user space program that communicates with the rootkit.c LKM. It passes a
* string to the LKM and reads the response from the LKM. For this example to work the device
* must be called /dev/rootkit.
*/

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#define BUFFER_LENGTH 256 ///< The buffer length (crude but fine)

static char receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM
void startPRU();
void uploadRootkit();

int main(int argc, char *argv[]){
    if (argc != 2) {
        printf("Sample count argument pwease");
        return errno;
    }

    int sample_count = atoi(argv[1]);
    if (sample_count <= 0 || sample_count > 1500) {
        printf("Sample count out of range");
        return errno;
    }

    // TODO write config with lkm write fn


    //uploads the LKM
    uploadRootkit();
    //starts the PRU, should most likely be moved to later in initialisation stage.
    startPRU();


    int ret, fd;
    char stringToSend[BUFFER_LENGTH];
    printf("Starting device...\n");
    fd = open("/dev/rootkit", O_RDWR); // Open the device with read/write access
    if (fd < 0){
        perror("Failed to open the device...");
        return errno;
    }

    // TODO in loop, poll periodically, delay

    printf("Reading from the device...\n");
    ret = read(fd, receive, BUFFER_LENGTH); // Read the response from the LKM
    if (ret < 0){
        perror("Failed to read the message from the device.");
        return errno;
    }
    printf("The received message is: [%s]\n", receive);
    printf("End of the program\n");
    return 0;
}

void uploadRootkit(){
    char buffer[128];
    // Change directory to ../LKM/ 
    FILE *pipe = popen("cd ../LKM && sh remove-rootkit.sh && sh upload-rootkit.sh", "r");
    
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            printf("%s", buffer); // Script already prints its own newlines
        }
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
}

void startPRU(){
    char buffer[128];
    // Change directory to ../pru/ 
    FILE *pipe = popen("cd ../pru && sh compile_script.sh && sh upload_firmware.sh", "r");
    
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            printf("%s", buffer); // Script already prints its own newlines
        }
        pclose(pipe);
    } else {
        perror("Failed to launch upload_firmware.sh");
    }
}
