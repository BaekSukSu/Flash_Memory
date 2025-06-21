#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "hybridmapping.h"

extern int fdd_read(int ppn, char *pagebuf);
extern int fdd_write(int ppn, char *pagebuf);
extern int fdd_erase(int pbn);


// address mapping table 정의
// 총 3개의 column 필요(lbn pbn offset)
typedef struct address_mapping_table
{
	int lbn;
	int pbn;
	int last_offset;
} Table;

// 연결 리스트에 들어갈 노드 정의
typedef struct node
{
	int pbn;
	struct node *next;
} Node;

// 연결 리스트 정의
typedef struct linked_list
{
	Node *head;
	Node *tail;
	int size;
} List;

void init_list(List *list)
{
	list->head = (Node *)malloc(sizeof(Node)); // dummy
	list->tail = (Node *)malloc(sizeof(Node)); // dummy

	list->head->next = list->tail;
	list->tail->next = NULL;

	list->size = 0;
}

void insert_node(List *list, Node *node)
{
	node->next = list->head->next;
	list->head->next = node;

	list->size++;
}

Node *delete_node(List *list) {
	Node *node = list->head->next;
	list->head->next = list->head->next->next;

    list->size--;

	return node;
}


// for debugging
void print_list(List *list)
{
	for(Node *n = list->head->next; n->next != NULL; n = n->next) {
		printf("pbn: %d -> ", n->pbn);
	}
	printf("\n");
}


// address mapping table 전역 변수로 선언
// linked list 전역 변수로 선언
Table address_mapping_table[BLOCKS_PER_DEVICE];
List *free_block_list;

void ftl_open()
{
	// address mapping table 초기화
	// lbn은 실제 pbn보다 1개 적어야 한다 = DATABLKS_PER_DEVICE
	for(int i=0; i<DATABLKS_PER_DEVICE; i++) {
		address_mapping_table[i].lbn = i;
		address_mapping_table[i].pbn = -1;
		address_mapping_table[i].last_offset = -1;
	}

	// free block linked list 초기화
	free_block_list = (List *)malloc(sizeof(List));
	init_list(free_block_list);

	// linked list에 node 추가
	// 오름차순으로 정렬 = 마지막 pbn부터 insert
	for(int i=BLOCKS_PER_DEVICE-1; i>-1; i--) {
		Node *n = (Node *)malloc(sizeof(Node));
		n->pbn = i;
		insert_node(free_block_list, n);
	}

	// 그 이외 필요한 작업 수행
	
	return;
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
void ftl_read(int lsn, char *sectorbuf)
{
	// lbn = 전달 받은 lsn이 속해있는 lbn
	// pbn = lbn과 대응되는 pbn(address mapping table 참고)
	// pbn_start = pbn에서 ppn의 시작 번호(page or sector 단위)
	int lbn = lsn / PAGES_PER_BLOCK;
	int pbn;
	int pbn_start;

	// address_mapping_table을 참고해서 lbn에 대응하는 pbn 찾기 & 해당 pbn의 ppn 시작 번호 찾기
	// address_mapping_table == 't'
	Table t = address_mapping_table[lbn];
	pbn = t.pbn;
	pbn_start = pbn * PAGES_PER_BLOCK;

	// backward scanning
	// flash memory에서 page 단위로 read
	// 각 page마다 spare 영역에 lsn이 저장되어 있음
	// 따라서 spare 영역만 읽어서 input으로 들어온 lsn과 spare에 있는 lsn을 비교
	for(int i=PAGES_PER_BLOCK-1; i>-1; i--) {
		char pagebuf[PAGE_SIZE];
		fdd_read(pbn_start+i, pagebuf);
		int read_lsn;
		memcpy(&read_lsn, pagebuf+SECTOR_SIZE, sizeof(read_lsn));
		if(lsn == read_lsn) {
			memcpy(sectorbuf, pagebuf, SECTOR_SIZE);
			break;
		}
	}

	return;
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
void ftl_write(int lsn, char *sectorbuf)
{
	// lbn = 전달 받은 lsn이 속해있는 lbn
	int lbn = lsn / PAGES_PER_BLOCK;

	// 전달 받은 sector buffer + lsn(binary int) = page buffer
	char pagebuf[PAGE_SIZE];
	memcpy(pagebuf, sectorbuf, SECTOR_SIZE);
	memcpy(pagebuf+SECTOR_SIZE, &lsn, sizeof(lsn));

	// address_mapping_table을 참고해서 lbn에 대응하는 pbn 찾기 & 해당 pbn의 ppn 시작 번호 찾기
	// address_mapping_table == 't'
	Table *t = &address_mapping_table[lbn];

	// 1. 해당 lbn에 할당된 pbn이 없는 경우
	if(t->pbn < 0) {
		// free_block_list 맨 앞에 있는 pbn을 할당 받음
		Node *n = delete_node(free_block_list);
		// address mapping table에서 lbn에 대응되는 pbn값 & last_offset값 수정
		t->pbn = n->pbn;
		t->last_offset++;
		// ppn에 작성
		int pbn_start = t->pbn * PAGES_PER_BLOCK;
		fdd_write(pbn_start + t->last_offset, pagebuf);

		free(n);
	}
	// 2. 해당 lbn에 할당된 pbn이 있는 경우
	else {
		// 2-1. 해당 pbn의 page가 아직 다 차지 않은 경우
		// 'last_offset = PAGES_PER_BLOCK-1' 이면 해당 block은 full
		if(t->last_offset < PAGES_PER_BLOCK-1) {
			int pbn_start = t->pbn * PAGES_PER_BLOCK;
			fdd_write(pbn_start + ++t->last_offset, pagebuf);
 		}
		// 2-2. 해당 pbn의 page가 다 찬 경우
		// 유효한 page만 복사 후 새로운 블럭에 붙여넣기
		else {
			// 1. linked list에서 새로운 block 하나 할당 받기
			Node *n = delete_node(free_block_list);
			int new_pbn = n->pbn;
			int new_pbn_start = new_pbn * PAGES_PER_BLOCK;
			free(n);

			// 2. 기존의 physical block에서 유효한 page들만 새로운 block에 복사
			// 유효한 page = 같은 lsn가 여러 개 존재한다면 그 중에서 가장 최신 page만 복사
			int pbn_start = t->pbn * PAGES_PER_BLOCK;
			int lsn_array[PAGES_PER_BLOCK]; // 해당 pbn에 저장되어 있는 lsn들을 담을 배열(lsn 번호값만 저장)
			for(int i = 0; i < PAGES_PER_BLOCK; i++)
				lsn_array[i] = -1;

			// backward scanning
			int idx = 0;
			int offset = -1;
			for(int i=PAGES_PER_BLOCK-1; i>-1; i--) {
				char copy_page[PAGE_SIZE];
				fdd_read(pbn_start+i, copy_page);

				// pagebuf에서 spare 부분만 추출 -> 해당 page의 lsn 파악 가능
				// lsn 번호값만 추출해서 lsn_array에 저장
				int read_lsn;
				memcpy(&read_lsn, copy_page+SECTOR_SIZE, sizeof(read_lsn));
				lsn_array[idx] = read_lsn;

				// 현재 입력으로 들어온 lsn과 ppn에 저장된 lsn이 동일할 경우 -> 굳이 복사할 필요 X
				// 지금 입력으로 들어온 lsn이 최신 data이기 때문에 기존 page 복사 X
				if(lsn != read_lsn) {
					int is_it_current = 0;
					// 첫번째로 scan하는 page는 pbn에서 맨 마지막에 있던 page
					// pbn에서 맨 마지막에 있던 page는 항상 최신 -> 바로 복사
					if(idx == 0)
						is_it_current = 1;
					// 두번째 scan하는 page부터는 그 전에 scan했던 page와 lsn이 겹치는지 판단
					// lsn_array에서 본인이 저장된 직전 index까지만 검사
					// 겹치는 lsn이 존재한다면 이번 page는 skip(이번 page가 더 오래된 page이기 때문)
					else {
						for(int j=0; j<idx; j++) {
							if(read_lsn == lsn_array[j]) {
								is_it_current = 0;
								break;
							}
							else
								is_it_current = 1;
						}
					}
					// 겹치는 lsn이 없으면 새롭게 할당 받은 block에 작성
					if(is_it_current) {
						fdd_write(new_pbn_start + ++offset, copy_page);
					}
				}
				idx++;
			}
			// 3. 이번에 write 요청으로 들어온 page를 새롭게 할당 받은 block에 작성
			// 이때 새롭게 할당 받은 block에는 기존 block에 있던 유효한 page 복사 붙여넣기 작업이 모두 완료된 상태
			fdd_write(new_pbn_start + ++offset, pagebuf);

			// 4. 기존 block erase 작업 & linked list 맨 앞에 추가
			fdd_erase(t->pbn);
			Node *erase_node = (Node *)malloc(sizeof(Node));
			erase_node->pbn = t->pbn;
			insert_node(free_block_list, erase_node);

			// 5. address mapping table에서 lbn에 대응하는 pbn, last_offset 수정
			t->pbn = new_pbn;
			t->last_offset = offset;
 		}
	}
	return;
}

// 
// Address mapping table 등을 출력하는 함수이며, 출력 포맷은 과제 설명서 참조
// 출력 포맷을 반드시 지켜야 하며, 그렇지 않는 경우 채점시 불이익을 받을 수 있음
//
void ftl_print()
{
	printf("lbn pbn last_offset\n");
	for(int i=0; i<DATABLKS_PER_DEVICE; i++) {
		printf("%d %d %d\n", address_mapping_table[i].lbn, address_mapping_table[i].pbn, address_mapping_table[i].last_offset);
	}

	return;
}