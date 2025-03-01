#include <stdio.h>
#include <stdlib.h>

// TYPE DEFINITIONS ------------------------------------------
struct node {
  int sign;
  int word;
  int link;
};
typedef struct node node;
#define SIGN(i) (nodes[(i)].sign)
#define QLINK(i) (nodes[(i)].word)
#define COUNT(i) (nodes[(i)].word)
#define TOP(i) (nodes[(i)].link)
#define SUC(i) (nodes[(i)].word)
#define NEXT(i) (nodes[(i)].link)
#define NUL 0

// CONSTANTS -------------------------------------------------
#define NODES 1000
#define LINELEN 20

// GLOBAL VARIABLES ------------------------------------------
node *nodes;
FILE *fp;
int N, N_old;
int j, k;
int F, R;
int P;
int AVAIL = NODES-1;

int main(int argc, char **argv) {
  nodes = malloc(NODES * sizeof(node));
  fp = fopen("in.txt", "r");

  char line[LINELEN+1];
  fgets(line, LINELEN, fp);
  N = atoi(line);
  N_old = N;

  // T1. Initialize
  for (int i = 1; i <= N; i++) {
    COUNT(i) = 0;
    TOP(i) = NUL;
  }

  while (fgets(line, LINELEN, fp) != NUL) {
    // T2. Next relation
    char *space;
    for (space = line; *space != ' '; space++);
    *space = '\0';
    j = atoi(line);
    k = atoi(space+1);

    if (AVAIL <= N) {
      printf("All %d nodes were used up :(\n", NODES);
      return 1;
    }
    // T3. Record the relation
    COUNT(k)++;
    SUC(AVAIL) = k;
    NEXT(AVAIL) = TOP(j);
    TOP(j) = AVAIL;
    AVAIL--;
  }

  // T4. Scan for zeros
  R = 0; QLINK(0) = 0;
  for (int i = 1; i <= N; i++) {
    if (COUNT(i) == 0) {
      QLINK(R) = i;
      R = i;
    }
  }
  F = QLINK(0);

  while (F != 0) {
    // T5. Output front of queue
    printf("%d ", F);
    N--;
    P = TOP(F);
    // T6. Erase relations
    while (P != NUL) {
      if (--COUNT(SUC(P)) == 0) {
	QLINK(R) = SUC(P);
	R = SUC(P);
      }
      P = NEXT(P);
    }
    TOP(F) = NUL;
    // T7. Remove from queue
    F = QLINK(F);
  };
  // T8. End of process
  printf("\n");

  // Loop detected?
  if (N > 0) {
    // Set QLINK of each unvisited node to a predecessor
    for (int i = 1; i <= N_old; i++) {
      if (TOP(i) == NUL) continue;
      F = i;
      int P = TOP(i);
      while (P != NUL) {
	QLINK(SUC(P)) = i;
	P = NEXT(P);
      }
    }

    // Trace the chain of predecessors to find a loop
    while (SIGN(F) == +0) {
      SIGN(F) = +1;
      F = QLINK(F);
    }

    // Trace the loop
    printf("Loop found: ");
    while (SIGN(F) == +1) {
      printf("%d <- ", F);
      SIGN(F) = 0;
      F = QLINK(F);
    }
    printf("%d\n", F);
  }

  fclose(fp);
  free(nodes);
}