#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include "common.h"
#include "wc.h"

/* Declare a struct node for the hash table */
struct node{
	int count; // the purpose of this program is to count the repetition of a word
	char *word; // to remember which word it saved
	struct node *next;
};

/* Forward declaration of structure for the function declarations below. */
struct wc {
	int table_size;
	struct node **words_table;
};

/* djb2 hash function found on http://www.cse.yorku.ca/~oz/hash.html#:~:text=If%20you%20just%20want%20to,K%26R%5B1%5D%2C%20etc. 
   Made small adjustment */
unsigned long hash(char *str, long table_size)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)){
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash % table_size;
}

//insert and count the word in wc
void wc_insert(struct wc *wc, char *the_word, unsigned long key){
	
	//first encountered this word
	if (wc->words_table[key] == NULL){
		wc->words_table[key] = (struct node *)malloc(sizeof(struct node));
		wc->words_table[key]->word = the_word;
		wc->words_table[key]->count = 1;
		wc->words_table[key]->next = NULL;
		return;
	}else{
		struct node * current = wc->words_table[key];
		struct node * prev = current;
		while (current != NULL){
			if (strcmp(the_word, current->word) == 0){
				current->count ++;
				free(the_word);
				return;
			}
			prev = current;
			current = current->next;
		}
		prev->next  = (struct node *)malloc(sizeof(struct node));
		prev->next->word = the_word;
		prev->next->count = 1;
		prev->next->next = NULL;
		return;
	}
}

/* Initialize wc data structure, returning pointer to it. The input to this
 * function is an array of characters. The length of this array is size.  The
 * array contains a sequence of words, separated by spaces. You need to parse
 * this array for words, and initialize your data structure with the words in
 * the array. You can use the isspace() function to look for spaces between
 * words. You can use strcmp() to compare for equality of two words. Note that
 * the array is read only and cannot be modified. */
struct wc *wc_init(char *word_array, long size)
{
	struct wc *wc;

	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);

	//allocate space for the hash table with a size of input_array size * 2
	wc->words_table = (struct node **)malloc(2 * size * sizeof(struct node *));
	assert(wc->words_table);

	//size of the hash table
	wc->table_size = 2 * size;

	long word_length = 0;

	for (long i = 0; i < size; i++){

		if (word_array[i] == '\0'){
			break;
		}
		
		if (word_length != 0 && word_array[i] != '\0' && isspace(word_array[i])){
			
			//plus one due to '\0'
			char *current_word = (char *)malloc((word_length + 1) * sizeof(char));
			strncpy(current_word, word_array + i - word_length, word_length);
			current_word[word_length] = '\0';

			unsigned long key = hash(current_word, wc->table_size);

			wc_insert(wc, current_word, key);

			word_length = 0;
	
		}else if (!isspace(word_array[i])){
			word_length++;
		}
		
	} 
	
	return wc;
}

/* wc_output produces output, consisting of unique words that have been inserted
 * in wc (in wc_init), and a count of the number of times each word has been
 * seen.
 *
 * The output should be sent to standard output, i.e., using the standard printf
 * function.
 *
 * The output should be in the format shown below. The words do not have to be
  * sorted in any order. Do not add any extra whitespace.
	word1:5
	word2:10
	word3:30
	word4:1
 */
void wc_output(struct wc *wc)
{
	unsigned long index = 0;
	while (index < wc->table_size){
		if (wc->words_table[index] != NULL){
			struct node* current = wc->words_table[index];
			while (current != NULL){
				printf("%s:%d\n",current->word, current->count);
				current = current -> next;
			}
		}
		index++;
	}
}

/* Destroy all the data structures you have created, so you have no memory
 * loss. */
void wc_destroy(struct wc *wc)
{
	unsigned long index = 0;
	while (index < wc->table_size){
		if (wc->words_table[index] != NULL){
			struct node* current = wc->words_table[index];
			struct node* next = current -> next;
			while (current != NULL){
				next = current -> next;
				free(current->word);
				free(current);
				current = next;
			}
		}
		index++;
	}
	free(wc->words_table);
	free(wc);
}
