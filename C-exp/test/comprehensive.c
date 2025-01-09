/**
 * @file comprehensive.c
 * @brief 学生信息管理系统
 *
 * 该程序实现了一个简单的学生信息管理系统，能够添加、查找、修改、删除和显示学生信息。
 * 学生信息包括姓名、ID和三门课程的成绩。系统还提供了对学生信息按平均成绩排序的功能。
 *
 * 主要功能包括：
 * - 从文件读取学生信息
 * - 添加新学生信息
 * - 查找学生信息
 * - 修改学生信息
 * - 删除学生信息
 * - 显示所有学生信息
 * - 按平均成绩排序学生信息（支持冒泡排序和选择排序）
 * - 将排序后的学生信息写入文件
 *
 */
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

/**
 * @brief 计算学生的平均分
 *
 * 该函数接收一个指向学生结构体的指针，并计算该学生三个成绩的平均值。
 *
 * @param student 指向包含三个成绩的学生结构体的指针
 * @return double 返回学生的平均成绩
 */
double calculateAverage(const struct Student* student) {
    return (student->scores[0] + student->scores[1] + student->scores[2]) / 3.0;
}

/**
 * @brief 对学生数组按平均成绩进行冒泡排序
 *
 * 该函数接收一个指向学生结构体数组的指针和数组的大小，通过比较相邻学生的平均成绩，
 * 将学生按平均成绩从低到高排序。
 *
 * @param students 指向学生结构体数组的指针
 * @param n 学生结构体数组的大小
 */
void bubbleSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
/**
 * @brief 对学生数组进行选择排序，根据学生的平均成绩进行升序排列。
 *
 * 该函数遍历学生数组，对于每个位置，找到从当前位置到数组末尾中
 * 平均成绩最低的学生，并将其与当前位置的学生交换。
 *
 * @param students 指向要排序的学生数组的指针。
 * @param n 学生数组的长度。
 */
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

/**
 * @brief 从指定文件中读取学生信息并存储到动态数组中。
 *
 * 该函数尝试打开一个包含学生信息的文件，并将文件中的每一行解析为一个学生结构体，
 * 然后将这些结构体存储到一个动态分配的数组中。函数返回成功读取的学生数量。
 *
 * @param filename 要读取的文件名。
 * @param students 指向学生结构体指针的指针，用于存储读取到的学生信息。
 * @return 成功读取的学生数量，如果发生错误则返回0。
 *
 * @note 文件中的每一行应包含学生的姓名、ID和三个分数，以空格分隔。
 *       如果文件无法打开或内存分配失败，函数将打印错误信息并返回0。
 */
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
/**
 * @brief 将学生信息写入文件
 *
 * 此函数将指定数量的学生信息写入给定的文件中。每个学生的信息包括姓名、ID 和三门课程的成绩。
 *
 * @param filename 要写入的文件名
 * @param students 指向学生结构体数组的指针
 * @param count 学生结构体的数量
 *
 * @return 无返回值。如果文件打开失败，会输出错误信息。
 */
            fclose(file);
            free(*students);
            *students = NULL;
            return 0;
    /**
    * @brief 显示学生信息的函数
    *
    * 此函数接收一个指向学生结构体数组的指针和学生数量，然后打印出每个学生的姓名、ID、三门成绩和平均成绩。
    * 成绩和平均成绩都以两位小数的形式显示。
    *
    * @param students 指向学生结构体数组的指针
    * @param count 学生数量
    */
        }
        *students = newStudents;
        (*students)[count++] = student;
    }

    fclose(file);
    return count;
}

/**
 * @brief 将学生信息写入文件
 *
 * 此函数将学生数组中的信息写入指定的文件中。每个学生的姓名、ID和三门成绩将以制表符分隔的形式存储在文件中。
 *
 * @param filename 目标文件的路径
 * @param students 指向学生结构体数组的指针
 * @param count 学生结构体数组中的元素数量
 *
 * @return 无返回值。如果文件无法打开，将输出错误信息。
 */
void writeStudentsToFile(const char* filename, const struct Student* students, int count) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file");
    /**
    * @brief 显示学生信息
    *
    * 此函数用于打印学生信息，包括姓名、ID、三门成绩以及平均成绩。
    * 它接受一个指向学生结构体数组的指针和学生数量作为参数。
    *
    * @param students 指向学生结构体数组的指针
    * @param count 学生数量
    */
        return;
    /**
    * @brief 添加学生信息到学生数组中
    *
    * 此函数用于向学生数组中添加一个新的学生记录。它会提示用户输入学生的姓名、ID 和三个分数，
    * 并将这些信息存储在动态分配的内存中。如果内存分配失败，函数会打印错误信息并返回。
    *
    * @param students 指向学生数组的指针的指针
    * @param count 指向学生数量的指针
    */
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

/**
 * @brief 添加学生信息到学生数组中
 *
 * 该函数用于向学生数组中添加一个新的学生信息。它首先提示用户输入学生的姓名、ID和三门课程的成绩，
 * 然后将这些信息存储在一个新的学生结构体中。接着，它会重新分配内存以容纳新的学生，并将新学生的信息
 * 添加到数组中。最后，更新学生计数。
 *
 * @param students 指向学生数组的指针
 * @param count 指向学生数量的指针
 *
 * @return 无返回值，但如果内存分配失败，会打印错误信息
 */
void addStudent(struct Student** students, int* count) {
    struct Student student;
    printf("Enter student name: ");
    scanf("%s", student.name);
    printf("Enter student ID: ");
    scanf("%s", student.id);
    printf("Enter three scores separated by spaces: ");
    scanf("%lf %lf %lf", &student.scores[0], &student.scores[1], &student.scores[2]);

    struct Student* newStudents = realloc(*students, (*count + 1) * sizeof(struct Student));
    /**
    * @brief 查找学生信息
    *
    * 该函数用于在给定的学生数组中查找特定ID的学生，并打印其详细信息。
    * 如果未找到匹配的学生，则输出"Student not found."。
    *
    * @param students 指向学生结构体数组的指针
    * @param count 学生数组中的学生数量
    */
    if (!newStudents) {
        perror("Memory allocation failed");
        return;
    }
    *students = newStudents;
    (*students)[*count] = student;
    (*count)++;
}

/**
 * @brief 查找并打印指定ID的学生信息
 *
 * 该函数接受一个学生结构体数组和数组的长度作为参数，
 * 提示用户输入要查找的学生ID，然后在数组中搜索匹配的ID。
 * 如果找到匹配的学生，打印学生的姓名、ID、各科成绩和平均成绩。
 * 如果未找到匹配的学生，打印“Student not found.”。
 *
 * @param students 指向学生结构体数组的指针
 * @param count 学生结构体数组的长度
 */
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

/**
 * @brief 修改学生信息
 *
 * 该函数用于修改指定学生的信息。用户可以输入学生的ID，然后根据ID查找学生并修改其姓名、ID和成绩。
 * 如果用户没有输入新的信息，则保留当前的信息。
 *
 * @param students 学生结构体数组指针
 * @param count 学生数量
 */
void modifyStudent(struct Student* students, int count) {
    char id[MAX_ID_LENGTH];
    printf("Enter student ID to modify: ");
    scanf("%s", id);

    for (int i = 0; i < count; ++i) {
        if (strcmp(students[i].id, id) == 0) {
            printf("Current student info:\n");
            printf("Name: %s, ID: %s, Scores: %.2f, %.2f, %.2f, Average: %.2f\n",
                   students[i].name, students[i].id, students[i].scores[0], students[i].scores[1], students[i].scores[2],
                   calculateAverage(&students[i]));

            printf("Enter new name (or press Enter to keep current): ");
            getchar(); // Consume newline character left in buffer
            fgets(students[i].name, MAX_NAME_LENGTH, stdin);
            students[i].name[strcspn(students[i].name, "\n")] = '\0'; // Remove newline character

            printf("Enter new ID (or press Enter to keep current): ");
            fgets(students[i].id, MAX_ID_LENGTH, stdin);
            students[i].id[strcspn(students[i].id, "\n")] = '\0'; // Remove newline character

            printf("Enter new scores separated by spaces (or press Enter to keep current): ");
            char line[100];
            fgets(line, sizeof(line), stdin);
            if (sscanf(line, "%lf %lf %lf", &students[i].scores[0], &students[i].scores[1], &students[i].scores[2]) != 3) {
                // If no valid input, keep the old scores
            }

            printf("Student modified successfully.\n");
            return;
        }
    }
    printf("Student not found.\n");
}

/**
 * @brief 删除指定ID的学生记录
 *
 * 该函数用于从学生数组中删除具有指定ID的学生记录。它首先提示用户输入要删除的学生ID，
 * 然后遍历学生数组查找匹配的ID。如果找到匹配项，则通过移动数组元素来覆盖该学生记录，
 * 并减少学生计数。最后，它尝试重新分配内存以适应新的学生数量。
 *
 * @param students 指向学生数组的指针的指针
 * @param count 指向学生数量的指针
 */
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

/**
 * @file comprehensive.c
 * @brief 学生信息管理系统的主程序文件
 *
 * 该文件包含了学生信息管理系统的主函数，负责处理用户输入并调用相应的功能模块。
 * 系统提供了添加、查找、修改、删除、显示和排序学生信息的功能，并能将数据保存到文件中。
 *
 * 主要功能包括：
 * - 从文件中读取学生信息
 * - 提供菜单选项供用户选择操作
 * - 调用相应的函数处理用户的选择
 * - 将修改后的学生信息保存到文件中
 *
 * @author 腾讯云 AI 代码助手
 * @date 2023-10-01
 */
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
                break;
            case 2:
                findStudent(students, count);
                break;
            case 3:
                modifyStudent(students, count);
                break;
            case 4:
                deleteStudent(&students, &count);
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



