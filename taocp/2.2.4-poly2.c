#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// ARENA CODE (credits to nullprogram) ---------------------------

typedef struct {
  char *cur;
  char *end;
} arena;

arena arenainit(size_t cap) {
  arena a;
  a.cur = malloc(cap);
  a.end = a.cur ? a.cur + cap : 0;
  return a;
}

void *arenaalloc(arena *a, size_t size, size_t align, size_t count) {
  ptrdiff_t padding = -(uintptr_t)a->cur & (align-1);
  ptrdiff_t available = a->end - a->cur - padding - count*size;
  if (available < 0)
    assert(0 && "Arena is full; aborting");
  void *p = a->cur + padding;
  a->cur += padding + count*size;
  return p;
}

#define new(a,t,n) (t *)arenaalloc(a, sizeof(t), _Alignof(t), n)
#define new1(a,t)  (t *)arenaalloc(a, sizeof(t), _Alignof(t), 1)

// END ARENA CODE ------------------------------------------------

// BEGIN MAIN CODE -----------------------------------------------

struct _node {
  struct _node *left;
  struct _node *right;
  struct _node *up;
  struct _node *down;
  int exp;
  union {
    char var;
    int c;
    int cv;   // If we don't care whether it represents a constant or variable
  };
};
typedef struct _node node;

void printpoly(node *P) {
  node *Q = P;
  while (1) {
    //// Get postorder successor of Q
    if (Q != P && Q->right->exp == 0) {  // P is last child of its parent?
      putchar(')');
      Q = Q->up;
    }
    else {
      if (Q != P) printf(" + ");
      for (Q = Q->right; Q->down; Q = Q->down)
	putchar('(');
    }

    if (Q == P) break;

    //// Print node
    if (!Q->down &&
	(Q->c != 1 || (Q->c == 1 && Q->exp == 0)))
      printf("%d", Q->c);

    if (Q->exp >= 1)
      printf("%c", Q->up->var);
    if (Q->exp > 1)
      printf("^%d", Q->exp);
  };
}

void addpoly(arena *a, node *P, node *Q) {
  node *R, *S;
findmatch:
  if (!P->down)
    for (; Q->down; Q = Q->down);
  else if (Q->var > P->var) {
    Q = Q->down;
    goto findmatch;
  }
  else if (Q->var == P->var) {
    P = P->down;
    Q = Q->down;
    goto findmatch;
  }
  else if (Q->var < P->var) {
    // Insert new term between Q and its children
    // The effect is to promote Q to a constant polynomial in P->var
    R = new1(a,node);
    //// Update the parent of Q's children to be R
    if ((S = Q->down)) {
      S->up = R;
      for (S = S->right; S->exp > 0; S = S->right)
	S->up = R;
    }
    R->left = R->right = R;
    R->up = Q;
    R->down = Q->down;
    R->cv = Q->cv;
    R->exp = 0;
    Q->cv = P->cv;
    Q->down = R;
    goto findmatch;
  }

  //// A match was found
  assert(!P->down && !Q->down);
  Q->c += P->c;
  if (Q->c == 0 && Q->exp != 0)
    goto delete;
  if (Q->exp == 0)
    goto syncQ;

advance:
  //// Advance to next exponent
  P = P->left;
  if (P->exp == 0)
    goto subtree_done;
  for (Q = Q->left; Q->exp > P->exp; Q = Q->left);
  if (Q->exp == P->exp)
    goto findmatch;
  else {
    //// Insert term with corresponding exponent to the right
    R = new1(a,node);
    R->up = Q->up;
    R->down = NULL;
    R->left = Q;
    R->right = Q->right;
    Q->right = R;
    R->right->left = R;
    R->exp = P->exp;
    R->cv = 0;
    Q = R;
    goto findmatch;
  }

subtree_done:
  P = P->up;
syncQ:
  if (!P->up) {
    //// We are done
    for (; Q->up; Q = Q->up);
    return;
  }
  for (; Q->up->cv != P->up->cv; Q = Q->up);
  goto advance;

delete:
  //// Delete zero term
  S = Q->left;
  Q = Q->right;
  S->right = Q;
  Q->left = S;
  if (P->left->exp != 0 && Q != S)
    goto advance;

  //// The polynomial rooted at Q->up only has a constant term,
  //// so we will convert it to a polynomial in Q->up->up->var
  R = Q;
  Q = Q->up;
  Q->down = R->down;
  Q->cv = R->cv;  // We destroy information about the old Q
  //// Update the parent of (old Q)'s children to be R
  if ((S = Q->down)) {
    S->up = R;
    for (S = S->right; S->exp > 0; S = S->right)
      S->up = R;
  }

  //// We might have created a zero term in the polynomial rooted at
  //// Q->up->up->var as a result
  if (!Q->down && Q->c == 0 && Q->exp != 0) {
    P = P->up;
    goto delete;
  }
  goto advance;
}

int main() {
  arena a = arenainit(2048);

  //// P = 3 + x^2 + xyz + z^3 - 3xz^3
  node *P = new1(&a,node);
  {
    node *z0     = new1(&a,node);
    node *z0x0   = new1(&a,node);
    node *z0x2   = new1(&a,node);
    node *z1     = new1(&a,node);
    node *z1y0   = new1(&a,node);
    node *z1y1   = new1(&a,node);
    node *z1y1x0 = new1(&a,node);
    node *z1y1x1 = new1(&a,node);
    node *z3     = new1(&a,node);
    node *z3x0   = new1(&a,node);
    node *z3x1   = new1(&a,node);

    P->up = NULL;
    P->down    = z0;     z0->up = z1->up = z3->up = P;
    z0->down   = z0x0;   z0x0->up = z0x2->up = z0;
    z1->down   = z1y0;   z1y0->up = z1y1->up = z1;
    z1y1->down = z1y1x0; z1y1x0->up = z1y1x1->up = z1y1;
    z3->down   = z3x0;   z3x0->up = z3x1->up = z3;
    z0x0->down = z0x2->down = z1y0->down = z1y1x0->down = z1y1x1->down = z3x0->down = z3x1->down = NULL;

    P->left = P->right = P;
    z3->right = z1->left = z0;
    z0->right = z3->left = z1;
    z1->right = z0->left = z3;
    z0x0->left = z0x0->right = z0x2;
    z0x2->left = z0x2->right = z0x0;
    z1y0->left = z1y0->right = z1y1;
    z1y1->left = z1y1->right = z1y0;
    z1y1x0->left = z1y1x0->right = z1y1x1;
    z1y1x1->left = z1y1x1->right = z1y1x0;
    z3x0->left = z3x0->right = z3x1;
    z3x1->left = z3x1->right = z3x0;

    P->exp      = 0; P->var = 'z';
    z0->exp     = 0; z0->var   = 'x';
    z1->exp     = 1; z1->var   = 'y';
    z3->exp     = 3; z3->var   = 'x';
    z0x0->exp   = 0; z0x0->c   = 3;
    z0x2->exp   = 2; z0x2->c   = 1;
    z1y0->exp   = 0; z1y0->c   = 0;
    z1y1->exp   = 1; z1y1->var = 'x';
    z1y1x0->exp = 0; z1y1x0->c = 0;
    z1y1x1->exp = 1; z1y1x1->c = 1;
    z3x0->exp   = 0; z3x0->c   = 1;
    z3x1->exp   = 1; z3x1->c   = -3;
  }

  //// Q = xy - x^2 - xyz - z^3 + 3xz^3
  node *Q = new1(&a, node);
  {
    node *z0     = new1(&a, node);
    node *z0y0   = new1(&a, node);
    node *z0y0x0 = new1(&a, node);
    node *z0y0x1 = new1(&a, node);
    node *z0y1   = new1(&a, node);
    node *z0y1x0 = new1(&a, node);
    node *z0y1x1 = new1(&a, node);
    node *z1     = new1(&a, node);
    node *z1y0   = new1(&a, node);
    node *z1y1   = new1(&a, node);
    node *z1y1x0 = new1(&a, node);
    node *z1y1x1 = new1(&a, node);
    node *z3     = new1(&a, node);
    node *z3x0   = new1(&a, node);
    node *z3x1   = new1(&a, node);

    Q->up = NULL;
    Q->down    = z0;     z0->up = z1->up = z3->up = Q;
    z0->down   = z0y0;   z0y0->up = z0y1->up = z0;
    z1->down   = z1y0;   z1y0->up = z1y1->up = z1;
    z3->down   = z3x0;   z3x0->up = z3x1->up = z3;
    z0y0->down = z0y0x0; z0y0x0->up = z0y0x1->up = z0y0;
    z0y1->down = z0y1x0; z0y1x0->up = z0y1x1->up = z0y1;
    z1y1->down = z1y1x0; z1y1x0->up = z1y1x1->up = z1y1;
    z0y0x0->down = z0y0x1->down = z0y1x0->down = z0y1x1->down = z1y0->down = z1y1x0->down = z1y1x1->down = z3x0->down = z3x1->down = NULL;

    Q->left = Q->right = Q;
    z3->right = z1->left = z0;
    z0->right = z3->left = z1;
    z1->right = z0->left = z3;
    z0y0->left = z0y0->right = z0y1;
    z0y1->left = z0y1->right = z0y0;
    z0y0x0->left = z0y0x0->right = z0y0x1;
    z0y0x1->left = z0y0x1->right = z0y0x0;
    z0y1x0->left = z0y1x0->right = z0y1x1;
    z0y1x1->left = z0y1x1->right = z0y1x0;
    z1y0->left = z1y0->right = z1y1;
    z1y1->left = z1y1->right = z1y0;
    z1y1x0->left = z1y1x0->right = z1y1x1;
    z1y1x1->left = z1y1x1->right = z1y1x0;
    z3x0->left = z3x0->right = z3x1;
    z3x1->left = z3x1->right = z3x0;

    Q->exp      = 0; Q->var    = 'z';
    z0->exp     = 0; z0->var   = 'y';
    z1->exp     = 1; z1->var   = 'y';
    z3->exp     = 3; z3->var   = 'x';
    z0y0->exp   = 0; z0y0->var = 'x';
    z0y1->exp   = 1; z0y1->var = 'x';
    z0y0x0->exp = 0; z0y0x0->c = 0;
    z0y0x1->exp = 2; z0y0x1->c = -1;
    z0y1x0->exp = 0; z0y1x0->c = 0;
    z0y1x1->exp = 1; z0y1x1->c = 1;
    z1y0->exp   = 0; z1y0->c   = 0;
    z1y1->exp   = 1; z1y1->var = 'x';
    z1y1x0->exp = 0; z1y1x0->c = 0;
    z1y1x1->exp = 1; z1y1x1->c = -1;
    z3x0->exp   = 0; z3x0->c   = -1;
    z3x1->exp   = 1; z3x1->c   = 3;
  }

  printf("P = "); printpoly(P); putchar('\n');
  printf("Q = "); printpoly(Q); putchar('\n');
  addpoly(&a, P, Q);
  printf("P+Q = "); printpoly(Q); putchar('\n');
  return 0;
}