#include <stdio.h>
#include <stdlib.h>

struct Student {
    char name[50];
    char id[11];
    float scores[3];
};

void inputStudentInfo(struct Student *student);
void writeStudentToFile(const struct Student *student, FILE *file);

int main() {
    struct Student student;
    FILE *file = fopen("e:\\aaa.txt", "w");
    
    if (file == NULL) {
        printf("无法打开文件！");
        return EXIT_FAILURE;
    }
    
    fprintf(file, "姓名\t学号\t科目1\t科目2\t科目3\n");
    fprintf(file, "------------------------------------------\n");
    
    while (1) {
        inputStudentInfo(&student);
        writeStudentToFile(&student, file);

        printf("Y 继续添加，N 退出: ");
        char choice;
        scanf(" %c", &choice); // Note the space before %c to consume newline character

        if (choice != 'Y' && choice != 'y') {
            break;
        }
    }
    
    fclose(file);
    printf("写入完成\n"); 
    
    return EXIT_SUCCESS;
}

void inputStudentInfo(struct Student *student) {
    printf("请输入学生的姓名: ");
    scanf("%s", student->name);
    
    printf("请输入学生的学号: ");
    scanf("%s", student->id);
    
    for (int i = 0; i < 3; i++) {
        printf("请输入第 %d 门课程的成绩: ", i + 1);
        scanf("%f", &student->scores[i]);
    }
}

void writeStudentToFile(const struct Student *student, FILE *file) {
    fprintf(file, "%s\t%s\t%.2f\t%.2f\t%.2f\n", 
            student->name, student->id, 
            student->scores[0], student->scores[1], student->scores[2]);
}
