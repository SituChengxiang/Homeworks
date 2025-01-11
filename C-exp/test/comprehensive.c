#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 100
#define MAX_ID_LENGTH 20

struct Student{
    char name[MAX_NAME_LENGTH];
    char id[MAX_ID_LENGTH];
    double scores[3];
};

double calculateAverage(const struct Student* student) {
    return (student->scores[0] + student->scores[1] + student->scores[2]) / 3.0;
}

void bubbleSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
            if (calculateAverage(&students[j]) > calculateAverage(&students[j + 1])) {
                struct Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
            }
        }
    }
}

void selectionSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; ++i) {
        int minIndex = i;
        for (int j = i + 1; j < n; ++j) {
            if (calculateAverage(&students[j]) < calculateAverage(&students[minIndex])) {
                minIndex = j;
            }
        }
        struct Student temp = students[i];
        students[i] = students[minIndex];
        students[minIndex] = temp;
    }
}

int readStudentsFromFile(const char* filename, struct Student** students) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return 0;
    }

    *students = NULL;
    int count = 0;
    struct Student student;

    while (fscanf(file, "%s %s %lf %lf %lf", student.name, student.id, &student.scores[0], &student.scores[1], &student.scores[2]) == 5) {
        struct Student* newStudents = realloc(*students, (count + 1) * sizeof(struct Student));
        if (!newStudents) {
            perror("Memory allocation failed");
            fclose(file);
            free(*students);
            *students = NULL;
            return 0;
        }
        *students = newStudents;
        (*students)[count++] = student;
    }

    fclose(file);
    return count;
}

void writeStudentsToFile(const char* filename, const struct Student* students, int count) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file");
    }

    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s\t%s\t%.2f\t%.2f\t%.2f\n", students[i].name, students[i].id,
                students[i].scores[0], students[i].scores[1], students[i].scores[2]);
    }

    fclose(file);
}

void displayStudents(const struct Student* students, int count) {
    printf("%-20s %-15s %-10s %-10s %-10s %-10s\n", "Name", "ID", "Score1", "Score2", "Score3", "Average");

    for (int i = 0; i < count; ++i) {
        printf("%-20s %-15s %-10.2f %-10.2f %-10.2f %-10.2f\n", students[i].name, students[i].id,
               students[i].scores[0], students[i].scores[1], students[i].scores[2],
               calculateAverage(&students[i]));
    }
}

void addStudent(struct Student** students, int* count) {
    struct Student student;
    printf("Enter student name: ");
    scanf("%s", student.name);
    printf("Enter student ID: ");
    scanf("%s", student.id);
    printf("Enter three scores separated by spaces: ");
    scanf("%lf %lf %lf", &student.scores[0], &student.scores[1], &student.scores[2]);

    struct Student* newStudents = realloc(*students, (*count + 1) * sizeof(struct Student));
    if (!newStudents) {
        perror("Memory allocation failed");
        return;
    }
    *students = newStudents;
    (*students)[*count] = student;
    (*count)++;
}
void findStudent(const struct Student* students, int count) {
    char id[MAX_ID_LENGTH];
    printf("Enter student ID to find: ");
    scanf("%s", id);

    for (int i = 0; i < count; ++i) {
        if (strcmp(students[i].id, id) == 0) {
            printf("Found student:\n");
            printf("Name: %s, ID: %s, Scores: %.2f, %.2f, %.2f, Average: %.2f\n",
                   students[i].name, students[i].id, students[i].scores[0], students[i].scores[1], students[i].scores[2],
                   calculateAverage(&students[i]));
            return;
        }
    }
    printf("Student not found.\n");
}

void modifyStudent(struct Student* students, int count) {
    char id[MAX_ID_LENGTH];
    printf("Enter student ID to modify: ");
    scanf("%s", id);
    getchar(); // 清除缓冲区

    for (int i = 0; i < count; ++i) {
        if (strcmp(students[i].id, id) == 0) {
            printf("Current student info:\n");
            printf("Name: %s, ID: %s, Scores: %.2f, %.2f, %.2f\n",
                   students[i].name, students[i].id, 
                   students[i].scores[0], students[i].scores[1], students[i].scores[2]);

            // 修改姓名
            char tempName[MAX_NAME_LENGTH];
            printf("Enter new name (or press Enter to keep current): ");
            fgets(tempName, MAX_NAME_LENGTH, stdin);
            if (tempName[0] != '\n') {
                tempName[strcspn(tempName, "\n")] = '\0';
                strcpy(students[i].name, tempName);
            }

            // 修改学号
            char tempId[MAX_ID_LENGTH];
            printf("Enter new ID (or press Enter to keep current): ");
            fgets(tempId, MAX_ID_LENGTH, stdin);
            if (tempId[0] != '\n') {
                tempId[strcspn(tempId, "\n")] = '\0';
                strcpy(students[i].id, tempId);
            }

            // 修改成绩
            printf("Enter new scores separated by spaces (or press Enter to keep current): ");
            char line[100];
            fgets(line, sizeof(line), stdin);
            if (line[0] != '\n') {
                double s1, s2, s3;
                if (sscanf(line, "%lf %lf %lf", &s1, &s2, &s3) == 3) {
                    students[i].scores[0] = s1;
                    students[i].scores[1] = s2;
                    students[i].scores[2] = s3;
                }
            }

            printf("Student modified successfully.\n");
            return;
        }
    }
    printf("Student not found.\n");
}
void deleteStudent(struct Student** students, int* count) {
    char id[MAX_ID_LENGTH];
    printf("Enter student ID to delete: ");
    scanf("%s", id);

    for (int i = 0; i < *count; ++i) {
        if (strcmp((*students)[i].id, id) == 0) {
            memmove(&(*students)[i], &(*students)[i + 1], (*count - i - 1) * sizeof(struct Student));
            (*count)--;
            struct Student* newStudents = realloc(*students, (*count) * sizeof(struct Student));
            if ((*count) == 0 || newStudents) {
                *students = newStudents;

            } else {
                perror("Memory reallocation failed");
            }
            printf("Student deleted successfully.\n");
            return;
        }
    }
    printf("Student not found.\n");
}
int main() {
    struct Student* students = NULL;
    int count = readStudentsFromFile("d:/students.txt", &students);

    int choice;
    do {
        printf("\n学生信息管理系统\n"
               "请选择您需要的操作：\n"
               "1. 添加学生信息\n"
               "2. 查找学生信息\n"
               "3. 修改学生信息\n"
               "4. 删除学生信息\n"
               "5. 显示所有学生信息\n"
               "6. 排序学生信息\n"
               "7. 退出\n");

        printf("请输入您的选择: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                addStudent(&students, &count);
                writeStudentsToFile("d:/students.txt", students, count);
                break;
            case 2:
                findStudent(students, count);
                break;
            case 3:
                modifyStudent(students, count);
                writeStudentsToFile("d:/students.txt", students, count);
                break;
            case 4:
                deleteStudent(&students, &count);
                writeStudentsToFile("d:/students.txt", students, count);
                break;
            case 5:
                displayStudents(students, count);
                break;
            case 6: {
                int sortChoice;
                printf("请选择排序算法：\n"
                       "1. 冒泡排序\n"
                       "2. 选择排序\n");
                printf("请输入您的选择: ");
                scanf("%d", &sortChoice);
                if (sortChoice == 1) {
                    bubbleSort(students, count);
                } else if (sortChoice == 2) {
                    selectionSort(students, count);
                } else {
                    printf("无效的选择。\n");
                }
                displayStudents(students, count);
                writeStudentsToFile("d:/sorted_students.txt", students, count);
                break;
            }
            case 7:
                printf("退出系统。\n");
                break;
            default:
                printf("无效的选择，请重新输入。\n");
        }
    } while (choice != 7);

    free(students);
    return 0;
}