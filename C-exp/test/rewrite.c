#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 学生结构体定义
struct Student {
    char name[50];
    char id[20];
    float score[3];
};

// 函数声明
double calculateAverage(const struct Student* student);
int readStudentFromFile(const char* filename, struct Student** students);
void addStudent(struct Student** students, int* count);
void findStudent(const struct Student* students, int count);
void modifyStudent(struct Student** students, int count);
void deleteStudent(struct Student** students, int* count);
void writeStudentsToFile(const char* filename, const struct Student* students, int count);
void displayStudents(const struct Student* students, int count);
void bubbleSort(struct Student* students, int n);
void selectionSort(struct Student* students, int n);

// 主函数
int main() {
    struct Student* students = NULL;
    int count = 0;
    int choice;

    // 首次读取文件
    count = readStudentFromFile("d:/students.txt", &students);

    while (1) {
        printf("\n学生信息管理系统\n"
               "1. 添加学生信息\n"
               "2. 查找学生信息\n"
               "3. 修改学生信息\n"
               "4. 删除学生信息\n"
               "5. 显示所有学生信息\n"
               "6. 排序学生信息\n"
               "7. 退出\n"
               "请输入您的选择: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("输入无效，请重试\n");
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
            case 1:
                addStudent(&students, &count);
                break;
            case 2:
                findStudent(students, count);
                break;
            case 3:
                modifyStudent(&students, count);
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
                       "2. 选择排序\n"
                       "请输入您的选择: ");
                scanf("%d", &sortChoice);
                
                if (sortChoice == 1) {
                    bubbleSort(students, count);
                } else if (sortChoice == 2) {
                    selectionSort(students, count);
                } else {
                    printf("无效的选择\n");
                    break;
                }
                displayStudents(students, count);
                writeStudentsToFile("d:/sorted_students.txt", students, count);
                break;
            }
            case 7:
                if (students != NULL) {
                    free(students);
                }
                printf("系统已退出\n");
                return 0;
            default:
                printf("无效的选择，请重试\n");
        }
    }
}

// 计算平均分
double calculateAverage(const struct Student* student) {
    return (student->score[0] + student->score[1] + student->score[2]) / 3.0;
}

// 从文件读取学生信息
int readStudentFromFile(const char* filename, struct Student** students) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("无法打开文件 %s\n", filename);
        return 0;
    }

    int count = 0;
    struct Student student;
    *students = NULL;

    while (fscanf(file, "%s %s %f %f %f", 
           student.name, student.id, 
           &student.score[0], &student.score[1], &student.score[2]) == 5) {
        
        struct Student* newStudents = realloc(*students, (count + 1) * sizeof(struct Student));
        if (newStudents == NULL) {
            printf("内存分配失败\n");
            free(*students);
            fclose(file);
            return 0;
        }
        
        *students = newStudents;
        (*students)[count] = student;
        count++;
    }

    fclose(file);
    return count;
}

// 添加学生信息
void addStudent(struct Student** students, int* count) {
    struct Student student;
    printf("请输入学生姓名: ");
    scanf("%s", student.name);
    printf("请输入学生学号: ");
    scanf("%s", student.id);
    printf("请输入三门课程成绩: ");
    scanf("%f %f %f", &student.score[0], &student.score[1], &student.score[2]);

    struct Student* newStudents = realloc(*students, (*count + 1) * sizeof(struct Student));
    if (newStudents == NULL) {
        printf("内存分配失败\n");
        return;
    }

    *students = newStudents;
    (*students)[*count] = student;
    (*count)++;

    writeStudentsToFile("d:/students.txt", *students, *count);
    printf("学生信息添加成功\n");
}

// 查找学生信息
void findStudent(const struct Student* students, int count) {
    char id[20];
    printf("请输入要查找的学生学号: ");
    scanf("%s", id);

    for (int i = 0; i < count; i++) {
        if (strcmp(students[i].id, id) == 0) {
            printf("%-20s %-15s %-10s %-10s %-10s %-10s\n", 
                   "姓名", "学号", "成绩1", "成绩2", "成绩3", "平均分");
            printf("%-20s %-15s %-10.2f %-10.2f %-10.2f %-10.2f\n",
                   students[i].name, students[i].id,
                   students[i].score[0], students[i].score[1], students[i].score[2],
                   calculateAverage(&students[i]));
            return;
        }
    }
    printf("未找到该学生\n");
}

// 修改学生信息
void modifyStudent(struct Student** students, int count) {
    char id[20];
    printf("请输入要修改的学生学号: ");
    scanf("%s", id);

    for (int i = 0; i < count; i++) {
        if (strcmp((*students)[i].id, id) == 0) {
            printf("请输入新的学生姓名: ");
            scanf("%s", (*students)[i].name);
            printf("请输入新的三门课程成绩: ");
            scanf("%f %f %f", &(*students)[i].score[0], 
                            &(*students)[i].score[1], 
                            &(*students)[i].score[2]);
            printf("学生信息修改成功\n");
            return;
        }
    }
    printf("未找到该学生\n");
}

// 删除学生信息
void deleteStudent(struct Student** students, int* count) {
    char id[20];
    printf("请输入要删除的学生学号: ");
    scanf("%s", id);

    for (int i = 0; i < *count; i++) {
        if (strcmp((*students)[i].id, id) == 0) {
            for (int j = i; j < *count - 1; j++) {
                (*students)[j] = (*students)[j + 1];
            }
            (*count)--;
            
            struct Student* newStudents = realloc(*students, *count * sizeof(struct Student));
            if (*count > 0 && newStudents != NULL) {
                *students = newStudents;
            }
            
            printf("学生信息删除成功\n");
            return;
        }
    }
    printf("未找到该学生\n");
}

// 显示所有学生信息
void displayStudents(const struct Student* students, int count) {
    printf("%-20s %-15s %-10s %-10s %-10s %-10s\n", 
           "姓名", "学号", "成绩1", "成绩2", "成绩3", "平均分");
    
    for (int i = 0; i < count; i++) {
        printf("%-20s %-15s %-10.2f %-10.2f %-10.2f %-10.2f\n",
               students[i].name, students[i].id,
               students[i].score[0], students[i].score[1], students[i].score[2],
               calculateAverage(&students[i]));
    }
}

// 冒泡排序
void bubbleSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (calculateAverage(&students[j]) > calculateAverage(&students[j + 1])) {
                struct Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
            }
        }
    }
}

// 选择排序
void selectionSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; i++) {
        int minIndex = i;
        for (int j = i + 1; j < n; j++) {
            if (calculateAverage(&students[j]) < calculateAverage(&students[minIndex])) {
                minIndex = j;
            }
        }
        if (minIndex != i) {
            struct Student temp = students[i];
            students[i] = students[minIndex];
            students[minIndex] = temp;
        }
    }
}

// 写入文件
void writeStudentsToFile(const char* filename, const struct Student* students, int count) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("无法打开文件 %s\n", filename);
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(file, "%s %s %.2f %.2f %.2f\n",
                students[i].name, students[i].id,
                students[i].score[0], students[i].score[1], students[i].score[2]);
    }

    fclose(file);
}
