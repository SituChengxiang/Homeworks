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
int findStudentById(const Student students[], int count, const char *id);
float calculateAverageScore(const Student *student);
void bubbleSortByAverageScore(Student students[], int count);

int main() {
    Student *students = NULL;
    int studentCount = 0;
    const char *filename = "e:\\aaa.txt";

    readStudentsFromFile(&students, &studentCount, filename);

    bubbleSortByAverageScore(students, studentCount);

    printf("姓名\t学号\t科目1\t科目2\t科目3\t平均分\n");
    for (int i = 0; i < studentCount; i++) {
        float avgScore = calculateAverageScore(&students[i]);
        printf("%s\t%s\t%.2f\t%.2f\t%.2f\t%.2f\n", 
               students[i].name, students[i].id, 
               students[i].scores[0], students[i].scores[1], students[i].scores[2], avgScore);
    }

    free(students);

    return EXIT_SUCCESS;
}

void readStudentsFromFile(Student **students, int *count, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("无法打开文件");
        return;
    }

    char line[256];
    int foundHeader = 0;

    while (fgets(line, sizeof(line), file)) {
        if (!foundHeader) {
            foundHeader = 1;
            fgets(line, sizeof(line), file); 
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        *students = (Student *)realloc(*students, (*count + 1) * sizeof(Student));
        if (*students == NULL) {
            perror("内存分配失败");
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

int findStudentById(const Student students[], int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(students[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

float calculateAverageScore(const Student *student) {
    return (student->scores[0] + student->scores[1] + student->scores[2]) / 3.0;
}

void bubbleSortByAverageScore(Student students[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            float avgScoreJ = calculateAverageScore(&students[j]);
            float avgScoreJPlusOne = calculateAverageScore(&students[j + 1]);
            if (avgScoreJ < avgScoreJPlusOne) {
                Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
            }
        }
    }
}




