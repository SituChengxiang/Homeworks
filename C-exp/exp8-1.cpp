#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Contact {
    char name[50];
    char phone[20];
};

int main() {
    int n;
    printf("输入联系人数： ");
    scanf("%d", &n);

    struct Contact *contacts = (struct Contact *)malloc(n * sizeof(struct Contact));
    if (contacts == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    while (getchar() != '\n');

    for (int i = 0; i < n; i++) {
        printf("输入第 %d 位联系人的姓名和电话：\n", i + 1);
        printf("姓名: ");
        fgets(contacts[i].name, sizeof(contacts[i].name), stdin);
        contacts[i].name[strcspn(contacts[i].name, "\n")] = '\0';
        printf("电话号码: ");
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

    printf("\n联系人的姓名和手机号为：:\n");
    for (int i = 0; i < n; i++) {
        printf("%s, \t %s\n", contacts[i].name, contacts[i].phone);
    }

    free(contacts);
    return 0;
}




