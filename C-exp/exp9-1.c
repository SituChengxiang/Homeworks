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
        printf("�޷����ļ���");
        return EXIT_FAILURE;
    }
    
    fprintf(file, "����\tѧ��\t��Ŀ1\t��Ŀ2\t��Ŀ3\n");
    fprintf(file, "------------------------------------------\n");
    
    while (1) {
        inputStudentInfo(&student);
        writeStudentToFile(&student, file);

        printf("Y ������ӣ�N �˳�: ");
        char choice;
        scanf(" %c", &choice); // Note the space before %c to consume newline character

        if (choice != 'Y' && choice != 'y') {
            break;
        }
    }
    
    fclose(file);
    printf("д�����\n"); 
    
    return EXIT_SUCCESS;
}

void inputStudentInfo(struct Student *student) {
    printf("������ѧ��������: ");
    scanf("%s", student->name);
    
    printf("������ѧ����ѧ��: ");
    scanf("%s", student->id);
    
    for (int i = 0; i < 3; i++) {
        printf("������� %d �ſγ̵ĳɼ�: ", i + 1);
        scanf("%f", &student->scores[i]);
    }
}

void writeStudentToFile(const struct Student *student, FILE *file) {
    fprintf(file, "%s\t%s\t%.2f\t%.2f\t%.2f\n", 
            student->name, student->id, 
            student->scores[0], student->scores[1], student->scores[2]);
}
