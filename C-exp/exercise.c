#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ����ѧ���ṹ��
struct Student {
	char name[100];    // �����ַ���
	char id[20];       // ѧ���ַ���
	float scores[3];   // ���ſγ̵ĳɼ�
};

int main() {
	struct Student student;
	FILE *file;

	// ��ʾ�û�����ѧ����������ѧ��
	printf("������ѧ����������");
	fgets(student.name, sizeof(student.name), stdin);
	student.name[strcspn(student.name, "\n")] = 0; // ȥ�����з�

	printf("������ѧ����ѧ�ţ�");
	fgets(student.id, sizeof(student.id), stdin);
	student.id[strcspn(student.id, "\n")] = 0; // ȥ�����з�

	// ��ʾ�û��������ſγ̵ĳɼ�
	for (int i = 0; i < 3; i++) {
		printf("�������%d�ſγ̵ĳɼ���", i + 1);
		scanf("%f", &student.scores[i]);
	}

	// ������뻺�����еĻ��з�
	getchar();

	// ���ļ�����д��
	file = fopen("d:\\aaa.txt", "w");
	if (file == NULL) {
		printf("�޷����ļ���");
		return 1;
	}

	// ��ѧ����Ϣд���ļ�
	fprintf(file, "������%s ѧ�ţ�%s", student.name, student.id);
	for (int i = 0; i < 3; i++) {
		fprintf(file, "��%d�ſγ̳ɼ���%f", i + 1, student.scores[i]);
	}

	// �ر��ļ�
	fclose(file);

	printf("ѧ����Ϣ�ѳɹ�д�뵽d:\\aaa.txt�ļ��С�");

	return 0;
}
