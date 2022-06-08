#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <io.h>

typedef struct tlistnode_s {
	struct tlistnode_s *prev, *next;
	void *data;
} TListNode;

typedef struct tlist_s {
	size_t node_size;
	TListNode *head, *tail;
} TList;

typedef bool(TLMatchFn)(void *data, void *args);

typedef struct timestamp_s {
	int16_t year;
	int8_t month;
	int8_t day;
	int8_t weekday;
	int8_t hour;
	int8_t min;
	int8_t sec;
} Timestamp;

enum UserGroup { User = 0, Manager, Admin };

enum Permission {
//! Service List
	BookService     = 0x0007,
	AccountService  = 0x0038,
	LibraryService  = 0x00C0,
	PropertyService = 0x0300,
	RecordService   = 0x0C00,
//! BookService
	Borrow          = 0b001,
	Return          = 0b010, 
	Query           = 0b100,
//! AccountService
	Register        = 0b001000,
	Login           = 0b010000,
	CancelAccount   = 0b100000,
//! LibraryService
	AddBook         = 0b01000000,
	ModifyBook      = 0b10000000,
//! PropertyService
	Recharge        = 0b0100000000,
	Deduct          = 0b1000000000,
//! RecordService
	NewRecord       = 0b010000000000,
	WithdrawRecord  = 0b100000000000,
//! UserGroup Access
	UserAccess      = BookService | AccountService | Recharge,
	ManagerAccess   = Query | LibraryService | RecordService,
	AdminAccess     = BookService | AccountService | LibraryService | PropertyService | RecordService,
};

typedef struct accountrecord_s {
	enum UserGroup group;  //@ �û���
	char account[16];      //@ �˻�
	char password[16];     //@ ����
	uint32_t hashkey;      //@ �˻���ϣ
	uint32_t id;           //@ ID
	int32_t amount;        //@ ���
	Timestamp tm_register; //@ ע��ʱ��
} AccountRecord;

typedef struct bookrecord_s {
	size_t stock;           //@ ����
	char ISBN[24];          //@ ISBN���
	char author[32];        //@ ����
	char name[64];          //@ ����
	Timestamp tm_introduce; //@ ����ʱ��
} BookRecord;

typedef struct borrowrecord_s {
	char ISBN[24];        //@ ISBN���
	uint32_t loan_time;   //@ ��������
	uint32_t borrower_id; //@ ������ID
	Timestamp tm_borrow;  //@ ���ʱ��
	Timestamp tm_return;  //@ �黹ʱ��
} BorrowRecord;

typedef struct librarydbinfo_s {
	uint16_t account_rec_size;
	uint16_t book_rec_size;
	uint16_t borrow_rec_size;
	uint16_t unused_1;
	uint32_t account_rec_num;
	uint32_t book_rec_num;
	uint32_t borrow_rec_num;
	uint32_t unused_2;
} LibraryDBInfo;

typedef struct librarydb_s {
	LibraryDBInfo header;
	TList *AccountRecords;
	TList *BookRecords;
	TList *BorrowRecords;
} LibraryDB;

typedef struct session_s {
	AccountRecord *host_ref;
	Timestamp tm_establish;
} Session, *SessionID;

typedef struct bootinfo_s {
	char root[256];
} BootInfo;

typedef struct librarysystem_s {
	char *db_path;
	LibraryDB database;
	SessionID session;
} LibSysDescription, *LibrarySystem;

/// ��������
uint32_t hash(const char *str) {
	register uint32_t hash_ = 5381;
	while (*str) {
		hash_ += (hash_ << 5) + *str++;
	}
	return hash_ & 0x7fffffff;
}

int getoption(const char *prompt) {
	char c;
	if (prompt != NULL) {
		printf(prompt);
	}
	while (isspace(c = getchar())) {}
	return c;
}

int getline(const char *prompt, char *buffer) {
	while (isblank(getchar())) {}
	if (prompt != NULL) {
		printf(prompt);
	}
	return scanf("%[^\n]", buffer);
}

void clear() {
	system("cls");
}

/// ͨ������֧��
TList* MakeTList(size_t node_size) {
	assert(node_size >= 1);
	TList *list = (TList*)calloc(1, sizeof(TList));
	list->node_size = node_size;
	return list;
}

void TLDestroy(TList *list) {
	if (!list || !list->head) return;
	TListNode *p = list->head, *q = list->tail;
	while (p != q) {
		p = p->next;
		free(p->prev);
	}
	free(q);
	list->head = list->tail = NULL;
}

void* TLAlloc(TList *list) {
	assert(list != NULL);
	return calloc(1, list->node_size);
}

void* TLAppend(TList *list, void *data) {
	assert(list != NULL);
	TListNode *node = (TListNode*)calloc(1, sizeof(TListNode));
	void *node_data = TLAlloc(list);
	if (data != NULL) {
		memcpy(node_data, data, list->node_size);
	}
	node->data = node_data;
	if (list->head != NULL) {
		node->prev = list->tail;
		list->tail->next = node;
		list->tail = node;
	} else {
		list->head = list->tail = node;
	}
	return node->data;
}

bool TLErase(TList *list, TListNode *node) {
	if (!list || !list->head || !node) return false;
	if (node->prev) {
		node->prev->next = node->next;
	} else if (node != list->head) {
		return false;
	} else {
		list->head = list->head->next;
	}
	if (node->next) {
		node->next->prev = node->prev;
	} else if (node != list->tail) {
		return false;
	} else if (list->tail) {
		list->tail = list->tail->prev;
	}
	return true;
}

//! ��ȫƥ�����
void* TLFind(TList *list, void *data, bool retnode) {
	if (!list || !list->head) return NULL;
	TListNode *node = list->head;
	while (node != NULL) {
		if (memcmp(node->data, data, list->node_size) == 0) {
			return retnode ? node : node->data;
		}
		node = node->next;
	}
	return NULL;
}

//! �Զ�������
void* TLMatch(TList *list, TLMatchFn match, void *args, bool retnode) {
	if (!list || !list->head) return NULL;
	TListNode *node = list->head;
	while (node != NULL) {
		if (match(node->data, args)) {
			return retnode ? node : node->data;
		}
		node = node->next;
	}
	return NULL;
}

/// ʱ�亯��
void TimeToTimestamp(Timestamp *stamp, time_t tm) {
	struct tm *detail = localtime(&tm);
	stamp->year = detail->tm_year + 1900;
	stamp->month = detail->tm_mon + 1;
	stamp->day = detail->tm_mday;
	stamp->weekday = detail->tm_wday + 1;
	stamp->hour = detail->tm_hour;
	stamp->min = detail->tm_min;
	stamp->sec = detail->tm_sec;
}

void GetTimestamp(Timestamp *stamp) {
	assert(stamp != NULL);
	time_t rawtime;
	time(&rawtime);
	TimeToTimestamp(stamp, rawtime);
}

double GetDuration(Timestamp *begin, Timestamp *end) {
	assert(begin != NULL);
	assert(end != NULL);

	time_t tm_begin, tm_end;
	struct tm detail = { };
	detail.tm_isdst = -1;

	detail.tm_year = begin->year - 1900;
	detail.tm_mon = begin->month - 1;
	detail.tm_mday = begin->day;
	detail.tm_wday = begin->weekday - 1;
	detail.tm_hour = begin->hour;
	detail.tm_min = begin->min;
	detail.tm_sec = begin->sec;
	tm_begin = mktime(&detail);

	detail.tm_year = end->year - 1900;
	detail.tm_mon = end->month - 1;
	detail.tm_mday = end->day;
	detail.tm_wday = end->weekday - 1;
	detail.tm_hour = end->hour;
	detail.tm_min = end->min;
	detail.tm_sec = end->sec;
	tm_end = mktime(&detail);

	return difftime(tm_end, tm_begin);
}

/// Ȩ�޹���
bool RequireService(enum UserGroup identity, enum Permission service) {
	enum Permission access[] = {
		[User] UserAccess, [Manager] ManagerAccess, [Admin] AdminAccess
	};
	return !!(access[identity] & service);
}

bool CheckAccess(enum UserGroup identity, enum Permission op) {
	enum Permission access[] = {
		[User] UserAccess, [Manager] ManagerAccess, [Admin] AdminAccess
	};
	return (access[identity] & op) == op;
}

/// ���ݹ���
bool OpenLibraryDB(LibraryDB *db, const char *path) {
	if (!db) return false;
	if (access(path, F_OK) != 0) {
		FILE *fp = fopen(path, "wb+");
		if (fp == NULL) return false;
		memset(&db->header, 0, sizeof(LibraryDBInfo));
		db->header.account_rec_size = sizeof(AccountRecord);
		db->header.book_rec_size = sizeof(BookRecord);
		db->header.borrow_rec_size = sizeof(BorrowRecord);
		db->header.account_rec_num = 1;
		fwrite(&db->header, sizeof(LibraryDBInfo), 1, fp);
		db->AccountRecords = MakeTList(sizeof(AccountRecord));
		db->BookRecords = MakeTList(sizeof(BookRecord));
		db->BorrowRecords = MakeTList(sizeof(BorrowRecord));

		AccountRecord admin = { };
		admin.group = Admin;
		admin.id = 1;
		strcpy(admin.account, "admin");
		strcpy(admin.password, "admin");
		admin.amount = 0;
		admin.hashkey = hash(admin.account);
		GetTimestamp(&admin.tm_register);
		TLAppend(db->AccountRecords, &admin);

		fclose(fp);
	} else {
		FILE *fp = fopen(path, "rb");
		if (fp == NULL) return false;
		fread(&db->header, sizeof(LibraryDBInfo), 1, fp);
		db->AccountRecords = MakeTList(sizeof(AccountRecord));
		db->BookRecords = MakeTList(sizeof(BookRecord));
		db->BorrowRecords = MakeTList(sizeof(BorrowRecord));
		for (int n = 0, size = db->header.account_rec_size; n < db->header.account_rec_num; ++n) {
			AccountRecord record;
			fread(&record, size, 1, fp);
			TLAppend(db->AccountRecords, &record);
		}
		for (int n = 0, size = db->header.book_rec_size; n < db->header.book_rec_num; ++n) {
			BookRecord record;
			fread(&record, size, 1, fp);
			TLAppend(db->BookRecords, &record);
		}
		for (int n = 0, size = db->header.borrow_rec_size; n < db->header.borrow_rec_num; ++n) {
			BorrowRecord record;
			fread(&record, size, 1, fp);
			TLAppend(db->BorrowRecords, &record);
		}
		db->header.account_rec_size = sizeof(AccountRecord);
		db->header.book_rec_size = sizeof(BookRecord);
		db->header.borrow_rec_size = sizeof(BorrowRecord);
	}
	return true;
}

bool ExportLibraryDB(LibraryDB *db, const char *path) {
	if (!db) return false;
	FILE *fp = fopen(path, "wb+");
	if (fp == NULL) return false;
	fwrite(&db->header, sizeof(LibraryDBInfo), 1, fp);
	for (TListNode *p = db->AccountRecords->head; p != NULL; p = p->next) {
		fwrite(p->data, db->AccountRecords->node_size, 1, fp);
	}
	for (TListNode *p = db->BookRecords->head; p != NULL; p = p->next) {
		fwrite(p->data, db->BookRecords->node_size, 1, fp);
	}
	for (TListNode *p = db->BorrowRecords->head; p != NULL; p = p->next) {
		fwrite(p->data, db->BorrowRecords->node_size, 1, fp);
	}
	fclose(fp);
	return true;
}

void CloseLibraryDB(LibraryDB *db) {
	TLDestroy(db->AccountRecords);
	TLDestroy(db->BookRecords);
	TLDestroy(db->BorrowRecords);
	db->AccountRecords = NULL;
	db->BookRecords = NULL;
	db->BorrowRecords = NULL;
}

/// �Ự������ҵ��
bool AccountHashMatch(AccountRecord *record, AccountRecord *info) {
	if (record->hashkey != info->hashkey) return false;
	if (strcmp(record->account, info->account) != 0) return false;
	return true;
}

bool AccountIDMatch(AccountRecord *record, uint32_t *pid) {
	return record->id == *pid;
}

bool ISBNMatch(BookRecord *record, const char *ISBN) {
	return strcmp(record->ISBN, ISBN) == 0;
}

bool ExclusiveLogin(LibrarySystem sys, const char *account, const char *password) {
	AccountRecord info = { }, *user = NULL;
	strcpy(info.account, account);
	info.hashkey = hash(account);
	user = (AccountRecord*)TLMatch(sys->database.AccountRecords, (void*)AccountHashMatch, &info, false);
	if (!user) return false;
	if (strcmp(user->password, password) != 0) return false;
	sys->session = (SessionID)calloc(1, sizeof(Session));
	sys->session->host_ref = user;
	Timestamp tm;
	GetTimestamp(&sys->session->tm_establish);
	return true;
}

void RegisterAccount(LibrarySystem sys, const char *account, const char *password) {
	AccountRecord record;
	strcpy(record.account, account);
	strcpy(record.password, password);
	record.hashkey = hash(account);
	record.group = User;
	record.id = (uint32_t)(rand() * rand());
	record.amount = 0;
	GetTimestamp(&record.tm_register);
	TLAppend(sys->database.AccountRecords, &record);
	++sys->database.header.account_rec_num;
}

int GetBorrowNum(LibrarySystem sys) {
	if (sys->session == NULL) return 0;
	int id = sys->session->host_ref->id, count = 0;
	TListNode *p = sys->database.BorrowRecords->head;
	while (p != NULL) {
		BorrowRecord *record = (BorrowRecord*)p->data;
		if (record->borrower_id == id && record->tm_return.year == -1) {
			++count;
		}
		p = p->next;
	}
	return count;
}

/// ����ҵ��
//! ��ʼ������Ϣ����
void SvrInitial(LibrarySystem sys) {
	puts(
"================" "\n"
"    ��ӭʹ��" "\n"
"  ͼ�����ϵͳ" "\n"
"================" "\n"
	);
}

//! ��¼����
void SvrLogin(LibrarySystem sys) {
	char opt = getoption(
"====ѡ��====" "\n"
"[1] ��¼"     "\n"
"[2] ע��"     "\n"
"[3] ����"     "\n"
"============" "\n"
"$ ");

	switch (opt) {
		case '1': {
			int nfailed = 0;
			while (true) {
				char account[64], password[64];
				getline("�˻���", account);
				getline("���룺", password);
				bool succeed = ExclusiveLogin(sys, account, password);
				if (succeed) {
					puts("��½�ɹ���");
					break;
				}
				puts("�˻����������");
				if (++nfailed == 3) {
					getoption("��ε�¼ʧ�ܣ��볢���һ����룡[Y]");
					break;
				}
				while (!isspace(getchar())) {}
			}
		}
		break;
		case '2': {
			char account[64], password[64], confirm[64];
			int nfailed = 0;
			while (true) {
				getline("�˻���", account);
				getline("���룺", password);
				getline("ȷ�����룺", confirm);
				AccountRecord record;
				strcpy(record.account, account);
				strcpy(record.password, password);
				record.hashkey = hash(account);
				if (strcmp(password, confirm) != 0) {
					puts("�������벻һ�£������ԣ�");
				} else if (TLMatch(sys->database.AccountRecords, (void*)AccountHashMatch, &record, false)) {
					puts("�˺��Ѵ��ڣ������ԣ�");
				} else {
					RegisterAccount(sys, account, password);
					puts("ע��ɹ���");
					break;
				}
				if (++nfailed == 3) {
					bool retry = true;
					while (retry) {
						retry = false;
						char opt = getoption("��⵽���ע��ʧ�ܣ��Ƿ������[Y/n] ");
						while (!isspace(getchar())) {}
						if (tolower(opt) == 'y') {
							nfailed = 0;
						} else if (tolower(opt) != 'n') {
							retry = true;
						}
					}
					if (nfailed == 3) break;
					clear();
				}
			}
		}
		break;
		case '3': {
			clear();
			return;
		}
		break;
		default: {
			puts("δ֪ѡ�");
		}
	}
}

//! ������ϢԤ������
void SvrDatacard(LibrarySystem sys) {
	AccountRecord *user = sys->session->host_ref;
	puts("================");
	printf("ID��%d\n", user->id);
	printf("�˻���%s\n", user->account);
	printf("���룺%s\n", user->password);
	printf("��%.2fԪ\n", user->amount * 0.01f);
	printf("������Ŀ��%d��\n", GetBorrowNum(sys));
	printf("ע��ʱ�䣺%4d-%02d-%02d %02d:%02d:%02d\n",
		user->tm_register.year, user->tm_register.month, user->tm_register.day,
		user->tm_register.hour, user->tm_register.min, user->tm_register.sec);
	printf("��һ�ε�¼ʱ�䣺%4d-%02d-%02d %02d:%02d:%02d\n",
		sys->session->tm_establish.year, sys->session->tm_establish.month, sys->session->tm_establish.day,
		sys->session->tm_establish.hour, sys->session->tm_establish.min, sys->session->tm_establish.sec);
	puts("================");
	getoption("�����������[Y] ");
}

//! �˻�ע������
void SvrCancelAccount(LibrarySystem sys) {
	if (sys->session->host_ref->id == 1) {
		puts("�޷�ɾ�����ù���Ա�˻�");
		return;
	}
	if (GetBorrowNum(sys) > 0) {
		puts("�����鼮δȫ���黹��ע�������Ѿܾ���");
	} else if (sys->session->host_ref->amount < 0) {
		puts("��ǰ�˻��ͻ���δ��ɣ�ע�������Ѿܾ���");
	} else {
		TListNode *node = TLFind(sys->database.AccountRecords, sys->session->host_ref, true);
		bool succeed = TLErase(sys->database.AccountRecords, node);
		if (succeed) {
			--sys->database.header.account_rec_num;
			free(sys->session);
			sys->session = NULL;
			puts("�˻�ע���ɹ���");
		} else {
			puts("δ֪�����˻�ע��ʧ�ܣ�");
		}
	}
}

//! ��ֵ����
void SvrRecharge(LibrarySystem sys) {
	char buffer[64];
	getline("��ֵ��", buffer);
	int amount = atoi(buffer);
	if (amount > 0) {
		sys->session->host_ref->amount += amount * 100;
	}
	puts(amount > 0 ? "��ֵ�ɹ���" : "��Ч��ֵ��");
}

//! �˻��������
void SvrAccountManage(LibrarySystem sys) {
	if (sys->session->host_ref->group != Admin) {
		puts("�˻��������δ��ǰ�û����ţ�");
		return;
	}
	while (sys->session != NULL) {
		char opt = getoption(
"====����====" "\n"
"[1] �û��б�" "\n"
"[2] �û�����" "\n"
"[3] ��������" "\n"
"[4] ע���û�" "\n"
"[5] ����" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				TListNode *p = sys->database.AccountRecords->head;
				puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
				puts(" ID �˻� ���� ��� ͼ������� ");
				while (p != NULL) {
					AccountRecord *backup = sys->session->host_ref;
					AccountRecord *record = (AccountRecord*)p->data;
					sys->session->host_ref = record;
					printf(" %d %s %s %.2fԪ %d��\n",
						record->id, record->account, record->password,
						record->amount * 0.01f, GetBorrowNum(sys));
					sys->session->host_ref = backup;
					p = p->next;
				}
				puts("[______________________________]");
			}
			break;
			case '2': {
				char account[16];
				getline("�û�����", account);
				AccountRecord record = { };
				strcpy(record.account, account);
				record.hashkey = hash(account);
				AccountRecord *user = TLMatch(sys->database.AccountRecords, (void*)AccountHashMatch, &record, false);
				if (user == NULL) {
					puts("������������ڣ�");
				} else {
					printf("�˻�ID��%d\n", user->id);
				}
			}
			break;
			case '3': {
				char sid[16];
				getline("�û�ID��", sid);
				uint32_t id = atoi(sid);
				TListNode *p = sys->database.AccountRecords->head;
				AccountRecord *target = NULL;
				while (p != NULL) {
					target = (AccountRecord*)p->data;
					if (target->id == id) {
						break;
					}
					target = NULL;
					p = p->next;
				}
				if (target == NULL) {
					puts("�˻������ڣ�");
				} else if (target->id == 1) {
					puts("�޷��������ù���Ա�˻������룡");
				} else {
					strcpy(target->password, "123456");
					printf("IDΪ%d���û�����������Ϊ\"123456\"\n", target->id);
				}
			}
			break;
			case '4': {
				char sid[16];
				getline("�û�ID��", sid);
				uint32_t id = atoi(sid);
				TListNode *p = sys->database.AccountRecords->head;
				AccountRecord *target = NULL;
				while (p != NULL) {
					target = (AccountRecord*)p->data;
					if (target->id == id) {
						break;
					}
					target = NULL;
					p = p->next;
				}
				if (target == NULL) {
					puts("�˻������ڣ�");
				} else if (target == sys->session->host_ref) {
					puts("�޷�ɾ����ǰ�˻���");
				} else {
					SessionID backup = (SessionID)calloc(1, sizeof(Session));
					memcpy(backup, sys->session, sizeof(Session));
					sys->session->host_ref = target;
					SvrCancelAccount(sys);
					if (sys->session == NULL) {
						sys->session = backup;
					} else {
						free(backup);
					}
				}
			}
			break;
			case '5': {
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! �û���ͼ����
void SvrAccountView(LibrarySystem sys) {
	while (sys->session != NULL) {
		char opt = getoption(
"====�˻�====" "\n"
"[1] �л��˺�" "\n"
"[2] ������Ϣ" "\n"
"[3] ע���˺�" "\n"
"[4] ��ֵ" "\n"
"[5] �˻���ѯ" "\n"
"[6] ����" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				SvrLogin(sys);
				return;
			}
			break;
			case '2': {
				SvrDatacard(sys);
			}
			break;
			case '3': {
				SvrCancelAccount(sys);
				if (sys->session == NULL) return;
			}
			break;
			case '4': {
				SvrRecharge(sys);
			}
			break;
			case '5': {
				SvrAccountManage(sys);
			}
			break;
			case '6': {
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ��Ŀ�������
void SvrBookList(LibrarySystem sys) {
	TListNode *p = sys->database.BookRecords->head;
	puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
	puts(" ISBN ���� ���� ���� ����ʱ��");
	while (p != NULL) {
		BookRecord *record = (BookRecord*)p->data;
		printf(" %s ��%s�� %s %d�� %4d-%02d-%02d\n",
			record->ISBN, record->name, record->author, record->stock,
			record->tm_introduce.year, record->tm_introduce.month, record->tm_introduce.day);
		p = p->next;
	}
	puts("[______________________________]");
}

//! ��Ŀ��ѯ����
void SvrSearchBook(LibrarySystem sys) {
	while (sys->session != NULL) {
		char opt = getoption(
"====����====" "\n"
"[1] ISBN" "\n"
"[2] ������ģ��������" "\n"
"[3] ���ߣ�ģ��������" "\n"
"[4] ����" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				char ISBN[64];
				getline("ISBN��ţ�", ISBN);
				BookRecord *record = TLMatch(sys->database.BookRecords, (void*)ISBNMatch, ISBN, false);
				if (record == NULL) {
					puts("�鼮�����ڣ�");
				} else {
					printf("��������%s�� ���ߣ�%s ������%d��\n",
						record->name, record->author, record->stock);
				}
			}
			break;
			case '2': {
				char partial_name[64];
				getline("������", partial_name);
				puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
				TListNode *p = sys->database.BookRecords->head;
				while (p != NULL) {
					BookRecord *record = (BookRecord*)p->data;
					if (strstr(record->name, partial_name) != NULL) {
						printf(" ISBN��%s ��������%s�� ���ߣ�%s ������%d��\n",
							record->ISBN, record->name, record->author, record->stock);
					}
					p = p->next;
				}
				puts("[______________________________]");
			}
			break;
			case '3': {
				char partial_name[64];
				getline("���ߣ�", partial_name);
				puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
				TListNode *p = sys->database.BookRecords->head;
				while (p != NULL) {
					BookRecord *record = (BookRecord*)p->data;
					if (strstr(record->author, partial_name) != NULL) {
						printf(" ISBN��%s ��������%s�� ���ߣ�%s ������%d��\n",
							record->ISBN, record->name, record->author, record->stock);
					}
					p = p->next;
				}
				puts("[______________________________]");
			}
			break;
			case '4': {
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ��Ŀ���ķ���
void SvrBorrow(LibrarySystem sys) {
	if (!CheckAccess(sys->session->host_ref->group, Borrow)) {
		puts("�鼮���ķ���δ��ǰ�û����ţ�");
		return;
	} else if (sys->session->host_ref->amount < 0) {
		puts("�鼮���ķ�������ǰ�û��رգ�������ͻ��Ѻ����ԣ�");
		return;
	}
	while (sys->session != NULL) {
		char ISBN[64], sday[64];
		int loan_time = 0;
		getline("ISBN��ţ�", ISBN);
		getline("����������", sday);
		BookRecord *book = TLMatch(sys->database.BookRecords, (void*)ISBNMatch, ISBN, false);
		if (book == NULL) {
			puts("�����鼮�����ڣ�");
		} else if (book->stock == 0) {
			puts("�����鼮���޴����");
		} else if ((loan_time = atoi(sday)) <= 0) {
			puts("��Ч�Ľ���������");
		} else {
			BorrowRecord record = { };
			strcpy(record.ISBN, book->ISBN);
			record.loan_time = loan_time;
			record.borrower_id = sys->session->host_ref->id;
			GetTimestamp(&record.tm_borrow);
			record.tm_return.year = -1; // unreturned mark
			TLAppend(sys->database.BorrowRecords, &record);
			++sys->database.header.borrow_rec_num;
			--book->stock;
			puts("���ĳɹ���");
		}
		if (tolower(getoption("�Ƿ�������ģ�[Y/n] ")) != 'y') break;
	}
}

//! ��Ŀ��������
void SvrNewBook(LibrarySystem sys) {
	if (!RequireService(sys->session->host_ref->group, LibraryService)) {
		puts("ͼ��������δ��ǰ�û����ţ�");
		return;
	}
	if (!CheckAccess(sys->session->host_ref->group, AddBook)) {
		puts("��ǰ�û���Ȩ�������Ŀ��");
		return;
	}
	while (sys->session != NULL) {
		char ISBN[64], name[64], author[64], snumber[64];
		int number = 0;
		getline("ISBN��ţ�", ISBN);
		getline("������", name);
		getline("���ߣ�", author);
		getline("������", snumber);

		BookRecord *book = TLMatch(sys->database.BookRecords, (void*)ISBNMatch, ISBN, false);
		if (book != NULL && (strcmp(book->name, name) != 0 || strcmp(book->author, author))) {
			puts("������Ŀ��������Ŀ��Ϣ��ͻ��������Ŀ��Ϣ���£�");
			printf("[ISBN��%s ��������%s�� ���ߣ�%s\n]\n");
		} else if ((number = atoi(snumber)) <= 0) {
			puts("������Ŀ��ĿӦ����Ϊһ����");
		} else if (book != NULL) {
			book->stock += number;
			puts("�鼮��Ŀ�Ѳ��䣡");
		} else {
			BookRecord record = { };
			strcpy(record.ISBN, ISBN);
			strcpy(record.name, name);
			strcpy(record.author, author);
			record.stock = number;
			GetTimestamp(&record.tm_introduce);
			TLAppend(sys->database.BookRecords, &record);
			++sys->database.header.book_rec_num;
			puts("��Ŀ��Ϣ��ӳɹ���");
		}

		if (tolower(getoption("�Ƿ������ӣ�[Y/n] ")) != 'y') break;
	}
}

//! �鼮��ͼ����
void SvrBookView(LibrarySystem sys) {
	while (sys->session != NULL) {
		char opt = getoption(
"====����====" "\n"
"[1] �鼮�б�" "\n"
"[2] �鼮����" "\n"
"[3] �����鼮" "\n"
"[4] ������Ŀ" "\n"
"[5] ����" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				SvrBookList(sys);
			}
			break;
			case '2': {
				SvrSearchBook(sys);
			}
			break;
			case '3': {
				SvrBorrow(sys);
			}
			break;
			case '4': {
				SvrNewBook(sys);
			}
			break;
			case '5': {
				clear();
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ���˽��ļ�¼��ͼ����
void SvrUserBorrowView(LibrarySystem sys) {
	while (sys->session != NULL) {
		puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
		puts(" ���� ISBN ���� ���� �������� �������� ");
		TListNode *p = sys->database.BorrowRecords->head;
		int index = 0;
		while (p != NULL) {
			BorrowRecord *record = (BorrowRecord*)p->data;
			if (record->borrower_id == sys->session->host_ref->id
				&& record->tm_return.year == -1) {
				BookRecord *book = TLMatch(sys->database.BookRecords,
					(void*)ISBNMatch, record->ISBN, false);
				printf(" [%d] %s ��%s�� %s %4d-%02d-%02d %d\n",
					++index, book->ISBN, book->name, book->author,
					record->tm_borrow.year, record->tm_borrow.month, record->tm_borrow.day,
					record->loan_time);
			}
			p = p->next;
		}
		puts("[______________________________]");
		char opt = getoption(
"====����====" "\n"
"[1] �黹" "\n"
"[2] ����" "\n"
"============" "\n"
"$ ");
		switch (opt) {
			case '1': {
				char sindex[16];
				getline("���黹��Ŀ������", sindex);
				int return_id = atoi(sindex);
				if (return_id < 0 || return_id > index) {
					puts("������Ŀ�����ڣ������ԣ�");
				} else {
					BorrowRecord *target = NULL;
					TListNode *p = sys->database.BorrowRecords->head;
					int index = 0;
					while (p != NULL) {
						BorrowRecord *record = (BorrowRecord*)p->data;
						if (record->borrower_id == sys->session->host_ref->id
							&& record->tm_return.year == -1) {
							BookRecord *book = TLMatch(sys->database.BookRecords,
								(void*)ISBNMatch, record->ISBN, false);
							if (++index == return_id) {
								target = record;
								break;
							}
						}
						p = p->next;
					}
					AccountRecord *borrower = TLMatch(sys->database.AccountRecords,
						(void*)AccountIDMatch, &target->borrower_id, false);
					BookRecord *book = TLMatch(sys->database.BookRecords,
						(void*)ISBNMatch, target->ISBN, false);
					GetTimestamp(&target->tm_return);
					double diff = GetDuration(&target->tm_return, &target->tm_borrow);
					int days = (int)(diff / 86400);
					if (days > target->loan_time) {
						borrower->amount -= (days - target->loan_time) * 0.3 * 100; // �0�60.3/day
						printf("���λ����ӳ�%d�죬����֧��%.2fԪ��\n", days - target->loan_time,
							(days - target->loan_time) * 0.3);
						if (borrower->amount < 0) {
							puts("�������㣬�뼰ʱ��ֵ������ͻ��ѣ�");
						}
					}
					++book->stock;
					puts("�鼮�黹�ɹ���");
				}
			}
			break;
			case '2': {
				clear();
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ���ļ�¼��ͼ����
void SvrBorrowRecords(LibrarySystem sys) {
	clear();
	puts("[^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^]");
	puts(" ISBN ���� ���� ������ �������� �������� �黹���� ");
	TListNode *p = sys->database.BorrowRecords->head;
	while (p != NULL) {
		BorrowRecord *record = (BorrowRecord*)p->data;
		AccountRecord *borrower = TLMatch(sys->database.AccountRecords,
			(void*)AccountIDMatch, &record->borrower_id, false);
		BookRecord *book = TLMatch(sys->database.BookRecords,
			(void*)ISBNMatch, record->ISBN, false);
		printf(" %s ��%s�� %s %s %d %4d-%02d-%02d ",
			book->ISBN, book->name, book->author, borrower->account, record->loan_time,
			record->tm_borrow.year, record->tm_borrow.month, record->tm_borrow.day);
		if (record->tm_return.year == -1) {
			printf("����");
		} else {
			printf("%4d-%02d-%02d", record->tm_return.year,
				record->tm_return.month, record->tm_return.day);
		}
		putchar('\n');
		p = p->next;
	}
	puts("[______________________________]");
}

//! ������ͼ����
void SvrBorrowView(LibrarySystem sys) {
	while (sys->session != NULL) {
		char opt = getoption(
"====����====" "\n"
"[1] ���ļ�¼" "\n"
"[2] ����" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				if (RequireService(sys->session->host_ref->group, RecordService)) {
					SvrBorrowRecords(sys);
				} else {
					SvrUserBorrowView(sys);
				}
			}
			break;
			case '2': {
				clear();
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ����˵�����
void SvrMenu(LibrarySystem sys) {
	while (sys->session != NULL) {
		char opt = getoption(
"====����====" "\n"
"[1] �˻�����" "\n"
"[2] ������Ŀ" "\n"
"[3] ������Ϣ" "\n"
"[4] �˳�" "\n"
"============" "\n"
"$ ");
		clear();
		switch (opt) {
			case '1': {
				SvrAccountView(sys);
			}
			break;
			case '2': {
				SvrBookView(sys);
			}
			break;
			case '3': {
				SvrBorrowView(sys);
			}
			break;
			case '4': {
				clear();
				return;
			}
			break;
			default: {
				puts("δ֪ѡ�");
			}
		}
	}
}

//! ��������
void SvrMain(LibrarySystem sys) {
	while (true) {
		while (sys->session == NULL) {
			puts("������ɵ�¼��");
			SvrLogin(sys);
			clear();
		}
		SvrMenu(sys);
		if (sys->session != NULL) break;
	}
}

/// ϵͳ�ۺ�
LibrarySystem Boot(BootInfo *info) {
	char buf[256];
	snprintf(buf, 256, "%s\\librecords.db", info->root);
	LibrarySystem sys = (LibrarySystem)calloc(1, sizeof(LibSysDescription));
	if (!OpenLibraryDB(&sys->database, buf)) {
		free(sys);
		return NULL;
	}
	sys->db_path = strdup(buf);

	time_t tm;
	time(&tm);
	srand(tm);

	return sys;
}

void Shutdown(LibrarySystem *sys) {
	ExportLibraryDB(&(*sys)->database, (*sys)->db_path);
	CloseLibraryDB(&(*sys)->database);
	free((*sys)->db_path);
	free(*sys);
	*sys = NULL;
}

void Run(LibrarySystem sys) {
	SvrInitial(sys);
	SvrMain(sys);
	puts("��������ֹ");
}

int main(int argc, char const *argv[])
{
	BootInfo info;
	getcwd(info.root, 256);
	LibrarySystem sys = Boot(&info);
	if (sys == NULL) {
		puts("����ʧ�ܣ�");
		return -1;
	}
	Run(sys);
	Shutdown(&sys);
	return 0;
}
