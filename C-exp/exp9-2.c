#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 50
#define MAX_ID_LENGTH 11

typedef struct {
    char name[MAX_NAME_LENGTH];
    char id[MAX_ID_LENGTH];
    float scores[3];
} Student;

void readStudentsFromFile(Student **students, int *count, const char *filename);
void writeStudentsToFile(const Student students[], int count, const char *filename);
int findStudentById(const Student students[], int count, const char *id);

int main() {
    Student *students = NULL;
    int studentCount = 0;
    const char *filename = "e:\\aaa.txt";

    readStudentsFromFile(&students, &studentCount, filename);

    while (1) {
        char idToDelete[MAX_ID_LENGTH];
        printf("������Ҫɾ����ѧ��ѧ��: ");
        scanf("%s", idToDelete);

        int index = findStudentById(students, studentCount, idToDelete);
        if (index != -1) {
            for (int i = index; i < studentCount - 1; i++) {
                students[i] = students[i + 1];
            }
            studentCount--;
            writeStudentsToFile(students, studentCount, filename);
            printf("ѧ����Ϣ�ѳɹ�ɾ����\n");
        } else {
            printf("δ�ҵ�ѧ����Ϣ��\n");
        }

        printf("Y ����ɾ����N �˳�: ");
        char choice;
        scanf(" %c", &choice); // Note the space before %c to consume newline character

        if (choice != 'Y' && choice != 'y') {
            break;
        }
    }

    // Free allocated memory
    free(students);

    return EXIT_SUCCESS;
}

void readStudentsFromFile(Student **students, int *count, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("�޷����ļ�");
        return;
    }

    char line[256];
    int foundHeader = 0;

    while (fgets(line, sizeof(line), file)) {
        if (!foundHeader) {
            foundHeader = 1; // Skip header line
            continue;
        }

        // Remove newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        *students = (Student *)realloc(*students, (*count + 1) * sizeof(Student));
        if (*students == NULL) {
            perror("�ڴ����ʧ��");
            fclose(file);
            return;
        }

        sscanf(line, "%[^,\t]\t%[^,\t]\t%f\t%f\t%f", 
               (*students)[*count].name, (*students)[*count].id, 
               &(*students)[*count].scores[0], &(*students)[*count].scores[1], &(*students)[*count].scores[2]);
        (*count)++;
    }

    fclose(file);
}

void writeStudentsToFile(const Student students[], int count, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("�޷����ļ�");
        return;
    }

    fprintf(file, "����\tѧ��\t��Ŀ1\t��Ŀ2\t��Ŀ3\n");

    for (int i = 0; i < count; i++) {
        fprintf(file, "%s\t%s\t%.2f\t%.2f\t%.2f\n", 
                students[i].name, students[i].id, 
                students[i].scores[0], students[i].scores[1], students[i].scores[2]);
    }

    fclose(file);
}

int findStudentById(const Student students[], int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(students[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}




