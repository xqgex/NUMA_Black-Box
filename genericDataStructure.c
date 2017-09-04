#include "genericDataStructure.h"

typedef struct node {
	int data;
	struct node * next;
} Node;
typedef struct list {
	Node * head;
} List;

void* Create();
bool Execute(void* ptr,char* op,void* args);
bool IsReadOnly(void* ptr,char* op);
int gds_str2int(char* str);
Node * createnode(int data);
bool add(int data,List* list);
bool rmv(int data,List* list);
bool display(List* list);
bool reverse(List* list);
bool destroy(List* list);


/**************************************************************/
/***************	Private Functions	***************/
/**************************************************************/
void* Create() {
	List* list = (List*) malloc(sizeof(List));
	list->head = NULL;
	return list;
}
bool Execute(void* ptr,char* op,void* args) {
	if (strncmp (op,"add",3) == 0) {
		return add(gds_str2int((char*)args),(List*)ptr);
	} else if (strncmp (op,"remove",6) == 0) {
		return rmv(gds_str2int((char*)args),(List*)ptr);
	} else if (strncmp (op,"display",7) == 0) {
		return display((List*)ptr);
	} else if (strncmp (op,"reverse",7) == 0) {
		return reverse((List*)ptr);
	} else if (strncmp (op,"destroy",7) == 0) {
		return destroy((List*)ptr);
	}
	return false;
}
bool IsReadOnly(void* ptr,char* op) {
	if (strncmp (op,"add",3) == 0) {
		return false;
	} else if (strncmp (op,"remove",6) == 0) {
		return false;
	} else if (strncmp (op,"display",7) == 0) {
		return true;
	} else if (strncmp (op,"reverse",7) == 0) {
		return false;
	} else if (strncmp (op,"destroy",7) == 0) {
		return false;
	}
	return false;
}
/**************************************************************/
/***************	Support Functions	***************/
/**************************************************************/
int gds_str2int(char* str) { /* Convert string to integer */
	int dec = 0;
	int i = 0;
	int len = 0;
	len = strlen(str);
	for (i=0;i<len;i++) {
		if ((48 <= str[i])&&(str[i] <= 57)) { /* If it's an integer, 48<=char<=57 */
			dec = dec*10 + (str[i]-'0');
		}
	}
	return dec;
}
Node * createnode(int data) {
	Node* newNode = (Node*) malloc(sizeof(Node));
	newNode->data = data;
	newNode->next = NULL;
	return newNode;
}
bool add(int data,List* list) {
	Node * current = NULL;
	if (list->head == NULL) {
		list->head = createnode(data);
	} else {
		current = list->head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = createnode(data);
	}
	return true;
}
bool rmv(int data,List* list) {
	Node * current = list->head;
	Node * previous = current;
	while (current != NULL) {
		if (current->data == data) {
			previous->next = current->next;
			if (current == list->head)
				list->head = current->next;
			free(current);
			return true;
		}
		previous = current;
		current = current->next;
	}
	return true;
}
bool display(List* list) {
	Node * current = list->head;
	if (list->head == NULL)
		return false;
	while (current->next != NULL) {
		printf("%d,", current->data);
		current = current->next;
	}
	printf("%d\n", current->data);
	return true;
}
bool reverse(List* list) {
	Node * reversed = NULL;
	Node * current = list->head;
	Node * temp = NULL;
	while (current != NULL) {
		temp = current;
		current = current->next;
		temp->next = reversed;
		reversed = temp;
	}
	list->head = reversed;
	return true;
}
bool destroy(List* list) {
	Node * current = list->head;
	Node * next = current;
	while (current != NULL) {
		next = current->next;
		free(current);
		current = next;
	}
	free(list);
	return true;
}
/**************************************************************/
/***************	Public Functions	***************/
/**************************************************************/
GenericDS* initializeDataStructure() {
	GenericDS* s_pointers = (GenericDS*) malloc(sizeof(GenericDS));
	s_pointers->Create = Create;
	s_pointers->Execute = Execute;
	s_pointers->IsReadOnly = IsReadOnly;
	return s_pointers;
}
