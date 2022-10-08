#include "common.h"
#include <string.h>
#include <stdbool.h> 
#include <ctype.h> 

int main(int argc, char **argv)
{
	if (argc <= 1){
		printf("Huh?\n");
	}else{
		bool digit_check = true;
		for (int i = 0; i < strlen(argv[1]); i++){
			if (!isdigit(argv[1][i])){
				digit_check = false;
			}
		}
		if (digit_check){
			float num = atof(argv[1]);
			int result = 1;
			if (num > 0 && num <= 12){
				for (int i = 1; i <= num; i++){
					result = result * i;
				}
				printf("%d", result);
				printf("\n");
			}else if (num > 12){
				printf("Overflow\n");
			}else{
				printf("Huh?\n");
			}
		}else{
			printf("Huh?\n");
		}
	}
	return 0;
}
