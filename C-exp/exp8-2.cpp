#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_STUDENTS 30
#define NUM_SUBJECTS 5

// ����ṹ��
struct Student {
    char name[50];
    int age;
    float scores[NUM_SUBJECTS];
};

// ��������ԭ��
void read(struct Student *x);
void write(struct Student x);
float calculateAverageScore(const struct Student *x);
void bubbleSort(struct Student arr[], int n);

int main() {
    struct Student students[NUM_STUDENTS];

    // ����ÿ��ѧ������Ϣ
    for (int i = 0; i < NUM_STUDENTS; i++) {
        printf("������� %d ��ͬѧ�ĳɼ�:\n", i + 1);
        read(&students[i]);
    }

    // ����ƽ���ɼ��Ӹߵ�������
    bubbleSort(students, NUM_STUDENTS);

    // �����������Ϣ
    printf("\n����ƽ���ɼ��Ӹߵ��������˳��Ϊ:\n");
    for (int i = 0; i < NUM_STUDENTS; i++) {
        write(students[i]);
    }

    return 0;
}

// �Զ������뺯��
void read(struct Student *x) {
    printf("��������: ");
    scanf("%s", x->name);
    printf("��������������: ");
    scanf("%d", &x->age);
    printf("������ %d �ſεĿ��Գɼ����ÿո�ָ���: ", NUM_SUBJECTS);
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        scanf("%f", &x->scores[i]);
    }
}

// �Զ����������
void write(struct Student x) {
    printf("����: %s\n", x.name);
    printf("����: %d\n", x.age);
    printf("�ɼ�: ");
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        printf("%.2f \t", x.scores[i]);
    }
    printf("\nƽ���ɼ�: %.2f\n",calculateAverageScore(&x));
    printf("-----------------\n");
}

float calculateAverageScore(const struct Student *x) {
    float sum = 0.0;
    for (int i = 0; i < NUM_SUBJECTS; i++) {
        sum += x->scores[i];
    }
    return sum / NUM_SUBJECTS;
}

// ð��������
void bubbleSort(struct Student arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (calculateAverageScore(&arr[j]) < calculateAverageScore(&arr[j + 1])) {
                // ����arr[j]��arr[j + 1]
                struct Student temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}



