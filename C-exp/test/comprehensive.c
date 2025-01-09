/**
 * @file comprehensive.c
 * @brief ѧ����Ϣ����ϵͳ
 *
 * �ó���ʵ����һ���򵥵�ѧ����Ϣ����ϵͳ���ܹ���ӡ����ҡ��޸ġ�ɾ������ʾѧ����Ϣ��
 * ѧ����Ϣ����������ID�����ſγ̵ĳɼ���ϵͳ���ṩ�˶�ѧ����Ϣ��ƽ���ɼ�����Ĺ��ܡ�
 *
 * ��Ҫ���ܰ�����
 * - ���ļ���ȡѧ����Ϣ
 * - �����ѧ����Ϣ
 * - ����ѧ����Ϣ
 * - �޸�ѧ����Ϣ
 * - ɾ��ѧ����Ϣ
 * - ��ʾ����ѧ����Ϣ
 * - ��ƽ���ɼ�����ѧ����Ϣ��֧��ð�������ѡ������
 * - ��������ѧ����Ϣд���ļ�
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
 * @brief ����ѧ����ƽ����
 *
 * �ú�������һ��ָ��ѧ���ṹ���ָ�룬�������ѧ�������ɼ���ƽ��ֵ��
 *
 * @param student ָ����������ɼ���ѧ���ṹ���ָ��
 * @return double ����ѧ����ƽ���ɼ�
 */
double calculateAverage(const struct Student* student) {
    return (student->scores[0] + student->scores[1] + student->scores[2]) / 3.0;
}

/**
 * @brief ��ѧ�����鰴ƽ���ɼ�����ð������
 *
 * �ú�������һ��ָ��ѧ���ṹ�������ָ�������Ĵ�С��ͨ���Ƚ�����ѧ����ƽ���ɼ���
 * ��ѧ����ƽ���ɼ��ӵ͵�������
 *
 * @param students ָ��ѧ���ṹ�������ָ��
 * @param n ѧ���ṹ������Ĵ�С
 */
void bubbleSort(struct Student* students, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
/**
 * @brief ��ѧ���������ѡ�����򣬸���ѧ����ƽ���ɼ������������С�
 *
 * �ú�������ѧ�����飬����ÿ��λ�ã��ҵ��ӵ�ǰλ�õ�����ĩβ��
 * ƽ���ɼ���͵�ѧ�����������뵱ǰλ�õ�ѧ��������
 *
 * @param students ָ��Ҫ�����ѧ�������ָ�롣
 * @param n ѧ������ĳ��ȡ�
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
 * @brief ��ָ���ļ��ж�ȡѧ����Ϣ���洢����̬�����С�
 *
 * �ú������Դ�һ������ѧ����Ϣ���ļ��������ļ��е�ÿһ�н���Ϊһ��ѧ���ṹ�壬
 * Ȼ����Щ�ṹ��洢��һ����̬����������С��������سɹ���ȡ��ѧ��������
 *
 * @param filename Ҫ��ȡ���ļ�����
 * @param students ָ��ѧ���ṹ��ָ���ָ�룬���ڴ洢��ȡ����ѧ����Ϣ��
 * @return �ɹ���ȡ��ѧ��������������������򷵻�0��
 *
 * @note �ļ��е�ÿһ��Ӧ����ѧ����������ID�������������Կո�ָ���
 *       ����ļ��޷��򿪻��ڴ����ʧ�ܣ���������ӡ������Ϣ������0��
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
 * @brief ��ѧ����Ϣд���ļ�
 *
 * �˺�����ָ��������ѧ����Ϣд��������ļ��С�ÿ��ѧ������Ϣ����������ID �����ſγ̵ĳɼ���
 *
 * @param filename Ҫд����ļ���
 * @param students ָ��ѧ���ṹ�������ָ��
 * @param count ѧ���ṹ�������
 *
 * @return �޷���ֵ������ļ���ʧ�ܣ������������Ϣ��
 */
            fclose(file);
            free(*students);
            *students = NULL;
            return 0;
    /**
    * @brief ��ʾѧ����Ϣ�ĺ���
    *
    * �˺�������һ��ָ��ѧ���ṹ�������ָ���ѧ��������Ȼ���ӡ��ÿ��ѧ����������ID�����ųɼ���ƽ���ɼ���
    * �ɼ���ƽ���ɼ�������λС������ʽ��ʾ��
    *
    * @param students ָ��ѧ���ṹ�������ָ��
    * @param count ѧ������
    */
        }
        *students = newStudents;
        (*students)[count++] = student;
    }

    fclose(file);
    return count;
}

/**
 * @brief ��ѧ����Ϣд���ļ�
 *
 * �˺�����ѧ�������е���Ϣд��ָ�����ļ��С�ÿ��ѧ����������ID�����ųɼ������Ʊ���ָ�����ʽ�洢���ļ��С�
 *
 * @param filename Ŀ���ļ���·��
 * @param students ָ��ѧ���ṹ�������ָ��
 * @param count ѧ���ṹ�������е�Ԫ������
 *
 * @return �޷���ֵ������ļ��޷��򿪣������������Ϣ��
 */
void writeStudentsToFile(const char* filename, const struct Student* students, int count) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file");
    /**
    * @brief ��ʾѧ����Ϣ
    *
    * �˺������ڴ�ӡѧ����Ϣ������������ID�����ųɼ��Լ�ƽ���ɼ���
    * ������һ��ָ��ѧ���ṹ�������ָ���ѧ��������Ϊ������
    *
    * @param students ָ��ѧ���ṹ�������ָ��
    * @param count ѧ������
    */
        return;
    /**
    * @brief ���ѧ����Ϣ��ѧ��������
    *
    * �˺���������ѧ�����������һ���µ�ѧ����¼��������ʾ�û�����ѧ����������ID ������������
    * ������Щ��Ϣ�洢�ڶ�̬������ڴ��С�����ڴ����ʧ�ܣ��������ӡ������Ϣ�����ء�
    *
    * @param students ָ��ѧ�������ָ���ָ��
    * @param count ָ��ѧ��������ָ��
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
 * @brief ���ѧ����Ϣ��ѧ��������
 *
 * �ú���������ѧ�����������һ���µ�ѧ����Ϣ����������ʾ�û�����ѧ����������ID�����ſγ̵ĳɼ���
 * Ȼ����Щ��Ϣ�洢��һ���µ�ѧ���ṹ���С����ţ��������·����ڴ��������µ�ѧ����������ѧ������Ϣ
 * ��ӵ������С���󣬸���ѧ��������
 *
 * @param students ָ��ѧ�������ָ��
 * @param count ָ��ѧ��������ָ��
 *
 * @return �޷���ֵ��������ڴ����ʧ�ܣ����ӡ������Ϣ
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
    * @brief ����ѧ����Ϣ
    *
    * �ú��������ڸ�����ѧ�������в����ض�ID��ѧ��������ӡ����ϸ��Ϣ��
    * ���δ�ҵ�ƥ���ѧ���������"Student not found."��
    *
    * @param students ָ��ѧ���ṹ�������ָ��
    * @param count ѧ�������е�ѧ������
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
 * @brief ���Ҳ���ӡָ��ID��ѧ����Ϣ
 *
 * �ú�������һ��ѧ���ṹ�����������ĳ�����Ϊ������
 * ��ʾ�û�����Ҫ���ҵ�ѧ��ID��Ȼ��������������ƥ���ID��
 * ����ҵ�ƥ���ѧ������ӡѧ����������ID�����Ƴɼ���ƽ���ɼ���
 * ���δ�ҵ�ƥ���ѧ������ӡ��Student not found.����
 *
 * @param students ָ��ѧ���ṹ�������ָ��
 * @param count ѧ���ṹ������ĳ���
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
 * @brief �޸�ѧ����Ϣ
 *
 * �ú��������޸�ָ��ѧ������Ϣ���û���������ѧ����ID��Ȼ�����ID����ѧ�����޸���������ID�ͳɼ���
 * ����û�û�������µ���Ϣ��������ǰ����Ϣ��
 *
 * @param students ѧ���ṹ������ָ��
 * @param count ѧ������
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
 * @brief ɾ��ָ��ID��ѧ����¼
 *
 * �ú������ڴ�ѧ��������ɾ������ָ��ID��ѧ����¼����������ʾ�û�����Ҫɾ����ѧ��ID��
 * Ȼ�����ѧ���������ƥ���ID������ҵ�ƥ�����ͨ���ƶ�����Ԫ�������Ǹ�ѧ����¼��
 * ������ѧ��������������������·����ڴ�����Ӧ�µ�ѧ��������
 *
 * @param students ָ��ѧ�������ָ���ָ��
 * @param count ָ��ѧ��������ָ��
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
 * @brief ѧ����Ϣ����ϵͳ���������ļ�
 *
 * ���ļ�������ѧ����Ϣ����ϵͳ�����������������û����벢������Ӧ�Ĺ���ģ�顣
 * ϵͳ�ṩ����ӡ����ҡ��޸ġ�ɾ������ʾ������ѧ����Ϣ�Ĺ��ܣ����ܽ����ݱ��浽�ļ��С�
 *
 * ��Ҫ���ܰ�����
 * - ���ļ��ж�ȡѧ����Ϣ
 * - �ṩ�˵�ѡ��û�ѡ�����
 * - ������Ӧ�ĺ��������û���ѡ��
 * - ���޸ĺ��ѧ����Ϣ���浽�ļ���
 *
 * @author ��Ѷ�� AI ��������
 * @date 2023-10-01
 */
int main() {
    struct Student* students = NULL;
    int count = readStudentsFromFile("d:/students.txt", &students);

    int choice;
    do {
        printf("\nѧ����Ϣ����ϵͳ\n"
               "��ѡ������Ҫ�Ĳ�����\n"
               "1. ���ѧ����Ϣ\n"
               "2. ����ѧ����Ϣ\n"
               "3. �޸�ѧ����Ϣ\n"
               "4. ɾ��ѧ����Ϣ\n"
               "5. ��ʾ����ѧ����Ϣ\n"
               "6. ����ѧ����Ϣ\n"
               "7. �˳�\n");

        printf("����������ѡ��: ");
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
                printf("��ѡ�������㷨��\n"
                       "1. ð������\n"
                       "2. ѡ������\n");
                printf("����������ѡ��: ");
                scanf("%d", &sortChoice);
                if (sortChoice == 1) {
                    bubbleSort(students, count);
                } else if (sortChoice == 2) {
                    selectionSort(students, count);
                } else {
                    printf("��Ч��ѡ��\n");
                }
                displayStudents(students, count);
                writeStudentsToFile("d:/sorted_students.txt", students, count);
                break;
            }
            case 7:
                printf("�˳�ϵͳ��\n");
                break;
            default:
                printf("��Ч��ѡ�����������롣\n");
        }
    } while (choice != 7);

    free(students);
    return 0;
}



