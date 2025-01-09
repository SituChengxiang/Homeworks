#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 定义学生结构体
typedef struct {
    char name[100];    // 姓名字符串
    char id[20];       // 学号字符串
    float scores[3];   // 三门课程的成绩
} Student;

int main() {
    Student student;
    FILE *file;

    // 提示用户输入学生的姓名和学号
    printf("请输入学生的姓名：");
    fgets(student.name, sizeof(student.name), stdin);
    student.name[strcspn(student.name, "\n")] = 0; // 去除换行符

    printf("请输入学生的学号：");
    fgets(student.id, sizeof(student.id), stdin);
    student.id[strcspn(student.id, "\n")] = 0; // 去除换行符

    // 提示用户输入三门课程的成绩
    for (int i = 0; i < 3; i++) {
        printf("请输入第%d门课程的成绩：", i + 1);
        scanf("%f", &student.scores[i]);
    }

    // 清除输入缓冲区中的换行符
    getchar();

    // 打开文件用于写入
    file = fopen("d:\\aaa.txt", "w");
    if (file == NULL) {
        printf("无法打开文件！");
        return 1;
    }

    // 将学生信息写入文件
    fprintf(file, "姓名：%s 学号：%s", student.name, student.id);
    for (int i = 0; i < 3; i++) {
        fprintf(file, "第%d门课程成绩：%f", i + 1, student.scores[i]);
    }

    // 关闭文件
    fclose(file);

    printf("学生信息已成功写入到d:\\aaa.txt文件中。");

    return 0;
}
