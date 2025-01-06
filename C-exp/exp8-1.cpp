#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Contact {
    char name[50];
    char phone[20];
};

int main() {
    int n;
    printf("������ϵ������ ");
    scanf("%d", &n);

    struct Contact *contacts = (struct Contact *)malloc(n * sizeof(struct Contact));
    if (contacts == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    while (getchar() != '\n');

    for (int i = 0; i < n; i++) {
        printf("����� %d λ��ϵ�˵������͵绰��\n", i + 1);
        printf("����: ");
        fgets(contacts[i].name, sizeof(contacts[i].name), stdin);
        contacts[i].name[strcspn(contacts[i].name, "\n")] = '\0';
        printf("�绰����: ");
        fgets(contacts[i].phone, sizeof(contacts[i].phone), stdin);
        contacts[i].phone[strcspn(contacts[i].phone, "\n")] = '\0';
    }


    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (strcmp(contacts[j].phone, contacts[j + 1].phone) > 0) {
                struct Contact temp = contacts[j];
                contacts[j] = contacts[j + 1];
                contacts[j + 1] = temp;
            }
        }
    }

    printf("\n��ϵ�˵��������ֻ���Ϊ��:\n");
    for (int i = 0; i < n; i++) {
        printf("%s, \t %s\n", contacts[i].name, contacts[i].phone);
    }

    free(contacts);
    return 0;
}




