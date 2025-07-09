/**
 * @file my_user.c
 * 
 * @author Anhton Nguyen
 * 
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_FILE_DIR "/dev/my_kernel"
#define WRITE_BUFF_SIZE 128
#define READ_BUFF_SIZE  1024

int num_racers;


struct ll {
    struct ll *next;
    char name[10];
    char school[10];
    char bib_num[10];
    char qualifier_time[10];
    char record[10];
};
// <name>,<school>,<bib_num>,<qualifier_time>,<record>  Anhton,GMU,1040,900
struct ll * init_node(struct ll *node, char *buffer){
    node = (struct ll*)malloc(sizeof(struct ll));
    memset(node->name, 0, 10);
    memset(node->school, 0, 10);
    memset(node->qualifier_time, 0, 10);
    memset(node->record, 0, 10);
    int comma_idx_1, comma_idx_2, comma_idx_3, count=1;
    for (int i=0; i<strlen(buffer); i++){
        if (buffer[i] == ','){
            if (count == 1){
                comma_idx_1 = i;
            }
            if (count == 2){
                comma_idx_2 = i;
            }
            if (count == 3){
                comma_idx_3 = i;
            }
            count++;
        }
    }
    char name[10];
    memset(name, 0, 10);
    memcpy(name, buffer, comma_idx_1);
    char school[10];
    memset(school, 0, 10);
    memcpy(school, buffer+comma_idx_1+1, comma_idx_2-comma_idx_1-1);
    char qualifier_time[10];
    memset(qualifier_time, 0, 10);
    memcpy(qualifier_time, buffer+comma_idx_2+1, comma_idx_3-comma_idx_2-1);
    char record[10];
    memset(record, 0, 10);
    memcpy(record, buffer+comma_idx_3+1, strlen(buffer)+1-comma_idx_3);
    strcpy(node->name, name);
    strcpy(node->school, school);
    strcpy(node->qualifier_time, qualifier_time);
    strcpy(node->record, record);
    node->next = NULL;
    return node;
}

void add_bib_num(struct ll* node, char *buffer){
    memset(node->bib_num, 0, 10);
    strcpy(node->bib_num, buffer); 
}

void insert_entry(struct ll *head, struct ll *node){
    if (head->next == NULL){
        head->next = node;
        return;
    }
    struct ll *curr = head;
    while (curr->next != NULL){
        curr = curr->next;
    }
    curr->next = node;
}
// if flag == 0
void print_node(struct ll *node, int i){
    printf("Racer %d (Bib Number: %s), (Name: %s), (School: %s), (Record: %s), (Qualifier Time: %s)\n",\
        i, node->bib_num, node->name, node->school, node->record, node->qualifier_time);
}

void display(struct ll *head){
    struct ll *curr = head;
    int i = 1;
    while (curr->next != NULL){
        curr = curr->next;
        print_node(curr, i);
        i++;
    }
    printf("\n");
}

int main(){
        uint8_t write_buf[WRITE_BUFF_SIZE];
        uint8_t read_buf[READ_BUFF_SIZE];
        struct ll *head, *n1, *n2, *n3, *n4;
        head = init_node(head, "head,head,head,head");
        int fd, idx=1;
        char option;
        fd = open(DEVICE_FILE_DIR, O_RDWR);
        if(fd < 0) {
                perror("");
                return 0;
        }
        printf("Welcome to Assignment 6!\n");
        while(1) {
                printf("\nChoose one of the following options:\n");
                printf("   1. Enter racer information\n");
                printf("   2. Simulate race\n");
                printf("   3. Exit\n");
                printf("Enter your choice here: ");
                scanf("  %[^\t\n]c", &option);
                
                switch(option) {
                        case '1':
                                printf("\nYou chose to enter racer information. Enter the number of racers: ");
                                scanf("%d", &num_racers);
                                while (idx <= num_racers){ 
                                        char buffer1[50];
                                        memset(buffer1, 0, 50);
                                        printf("Enter racer %d <name>,<school>,<qualifier_time>,<record>: ", idx);
                                        scanf("%s", buffer1);
                                        printf("Enter racer %d <bib_number>: ", idx);
                                        scanf("  %[^\t\n]d\n",write_buf);
                                        write(fd, write_buf, strlen(write_buf)+1);
                                        switch(idx-1){
                                        case 0:
                                                n1 = init_node(n1, buffer1);
                                                add_bib_num(n1, write_buf);
                                                insert_entry(head, n1);
                                                break;
                                        case 1:
                                                n2 = init_node(n2, buffer1);
                                                add_bib_num(n2, write_buf);
                                                insert_entry(head, n2);
                                                break;
                                        case 2:
                                                n3 = init_node(n3, buffer1);
                                                add_bib_num(n3, write_buf);
                                                insert_entry(head, n3);
                                                break;
                                        case 3:
                                                n4 = init_node(n4, buffer1);
                                                add_bib_num(n4, write_buf);
                                                insert_entry(head, n4);
                                                break;
                                        default: break;
                                        }
                                        memset(write_buf, 0, WRITE_BUFF_SIZE); 
                                        idx++;
                                }
                                break;
                        case '2':
                                write(fd, "!", 2);                            /* start race */
                                printf("\nYou chose to simulate the race.\n");
                                printf("Press 'q' to stop simulating race: ");
                                scanf("  %[^\t\n]c\n", write_buf);            /* press 'q' to stop race */
                                write(fd, write_buf, strlen(write_buf)+1);
                                printf("\nRACER INFORMATION:\n");
                                display(head);
                                read(fd, read_buf, READ_BUFF_SIZE);
                                printf("\n%s", read_buf);
                                close(fd);
                                return 0;
                                break;
                        case '3':
                                printf("\nEnding user application.\n");
                                close(fd);
                                return 0;
                        default: break;
                }
        }
        return 0;
}