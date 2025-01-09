#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_STUDENTS 30
#define NUM_SUBJECTS 5

// 定义结构体
struct Student {
    char name[50];
    int age;
    float scores[NUM_SUBJECTS];
};

// 声明函数原型
void read(struct Student *x);
void write(struct Student x);
float calculateAverageScore(const struct Student *x);
void bubbleSort(struct Student arr[], int n);

int main() {
    struct Student students[NUM_STUDENTS];

    // 输入每个学生的信息
    for (int i = 0; i < NUM_STUDENTS; i++) {
        printf("请输入第 %d 个同学的成绩:\n", i + 1);
        read(&students[i]);
    }

    // 按照平均成绩从高到低排序
    bubbleSort(students, NUM_STUDENTS);

    // 输出排序后的信息
    printf("\n按照平均成绩从高到低输出的顺序为:\n");
    for (int i = 0; i < NUM_STUDENTS; i++) {
        write(students[i]);
    }

    return 0;
}

// 自定义输入函数
void read(struct Student *x) {
    printf("输入姓名: ");
    scanf("%s", x->name);
    printf("请输入他的年龄: ");
    scanf("%d", &x->age);
    printf("请输入 %d 门课的考试成绩（用空格分隔）: ", NUM_SUBJECTS);
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        scanf("%f", &x->scores[i]);
    }
}

// 自定义输出函数
void write(struct Student x) {
    printf("姓名: %s\n", x.name);
    printf("年龄: %d\n", x.age);
    printf("成绩: ");
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        printf("%.2f \t", x.scores[i]);
    }
    printf("\n平均成绩: %.2f\n",calculateAverageScore(&x));
    printf("-----------------\n");
}

float calculateAverageScore(const struct Student *x) {
    float sum = 0.0;
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        sum += x->scores[i];
    }
    return sum / NUM_SUBJECTS;
}

// 冒泡排序函数
void bubbleSort(struct Student arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (calculateAverageScore(&arr[j]) < calculateAverageScore(&arr[j + 1])) {
                // 交换arr[j]和arr[j + 1]
                struct Student temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}



