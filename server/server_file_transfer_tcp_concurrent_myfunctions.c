#include <stdio.h>
#include <time.h>

int myfunc(){
	fprintf(stdout, "Function. Continuing to next loop\n");
	continue;
}


int main(){
	
	int i = 0;
	while(1){
		fprintf(stdout, "Iteration number: %d\n", i+1);
		sleep(1);
		myfunc();
		fprintf(stdout, "second block");
		
	}
	
}
